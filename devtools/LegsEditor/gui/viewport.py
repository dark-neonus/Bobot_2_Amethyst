"""
Viewport Module
3D rendering using ModernGL
"""

import moderngl
import numpy as np
from typing import Dict, List, Optional
import struct

from gui.utils import Camera, perspective_matrix


class Viewport:
    """3D viewport for rendering the robot"""
    
    def __init__(self, ctx: moderngl.Context):
        self.ctx = ctx
        self.camera = Camera()
        
        # Framebuffer for render-to-texture
        self.fbo = None
        self.texture = None
        self.width = 800
        self.height = 600
        
        # Shader programs
        self.line_program = None
        self.grid_program = None
        
        # Vertex buffers
        self.grid_vao = None
        self.robot_vao = None
        
        self._setup_shaders()
        self._setup_grid()
        self.resize(self.width, self.height)
    
    def _setup_shaders(self):
        """Create shader programs"""
        # Simple line shader
        line_vertex_shader = """
        #version 330
        
        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 model;
        
        in vec3 in_position;
        in vec3 in_color;
        
        out vec3 v_color;
        
        void main() {
            gl_Position = projection * view * model * vec4(in_position, 1.0);
            v_color = in_color;
        }
        """
        
        line_fragment_shader = """
        #version 330
        
        in vec3 v_color;
        out vec4 fragColor;
        
        void main() {
            fragColor = vec4(v_color, 1.0);
        }
        """
        
        self.line_program = self.ctx.program(
            vertex_shader=line_vertex_shader,
            fragment_shader=line_fragment_shader
        )
    
    def _setup_grid(self):
        """Create grid floor"""
        grid_lines = []
        grid_size = 500
        grid_step = 50
        grid_color = [0.3, 0.3, 0.3]
        
        # Lines parallel to X axis
        for z in range(-grid_size, grid_size + 1, grid_step):
            grid_lines.extend([
                -grid_size, 0, z, *grid_color,
                grid_size, 0, z, *grid_color
            ])
        
        # Lines parallel to Z axis
        for x in range(-grid_size, grid_size + 1, grid_step):
            grid_lines.extend([
                x, 0, -grid_size, *grid_color,
                x, 0, grid_size, *grid_color
            ])
        
        # Main axes (brighter)
        # X axis - red
        grid_lines.extend([
            -grid_size, 0, 0, 1.0, 0.0, 0.0,
            grid_size, 0, 0, 1.0, 0.0, 0.0
        ])
        
        # Z axis - blue
        grid_lines.extend([
            0, 0, -grid_size, 0.0, 0.0, 1.0,
            0, 0, grid_size, 0.0, 0.0, 1.0
        ])
        
        # Y axis - green
        grid_lines.extend([
            0, 0, 0, 0.0, 1.0, 0.0,
            0, 200, 0, 0.0, 1.0, 0.0
        ])
        
        grid_data = np.array(grid_lines, dtype='f4')
        vbo = self.ctx.buffer(grid_data.tobytes())
        self.grid_vao = self.ctx.vertex_array(
            self.line_program,
            [(vbo, '3f 3f', 'in_position', 'in_color')]
        )
    
    def resize(self, width: int, height: int):
        """Resize framebuffer"""
        self.width = max(1, width)
        self.height = max(1, height)
        
        # Create new texture and framebuffer
        if self.texture:
            self.texture.release()
        if self.fbo:
            self.fbo.release()
        
        self.texture = self.ctx.texture((self.width, self.height), 4)
        self.texture.filter = (moderngl.LINEAR, moderngl.LINEAR)
        
        depth_attachment = self.ctx.depth_renderbuffer((self.width, self.height))
        self.fbo = self.ctx.framebuffer(
            color_attachments=[self.texture],
            depth_attachment=depth_attachment
        )
    
    def render(self, leg_positions: Dict[int, List[np.ndarray]], 
               body_corners: List[np.ndarray]):
        """Render the scene"""
        if not self.fbo:
            print("ERROR: FBO not initialized!")
            return
            
        self.fbo.use()
        # Clear with a slightly different color to verify rendering is working
        self.ctx.clear(0.2, 0.2, 0.25, 1.0)
        self.ctx.enable(moderngl.DEPTH_TEST)
        
        # Check OpenGL errors
        error = self.ctx.error
        if error != 'GL_NO_ERROR':
            print(f"OpenGL Error before rendering: {error}")
        
        self.ctx.line_width = 2.0
        
        # Setup matrices
        aspect = self.width / self.height
        projection = perspective_matrix(60.0, aspect, 0.1, 5000.0)
        view = self.camera.get_view_matrix()
        model = np.eye(4, dtype=np.float32)
        
        self.line_program['projection'].write(projection.tobytes())
        self.line_program['view'].write(view.tobytes())
        self.line_program['model'].write(model.tobytes())
        
        # Render grid
        if self.grid_vao:
            self.grid_vao.render(moderngl.LINES)
        
        # Render body box
        if body_corners:
            self._render_body(body_corners)
        
        # Render legs
        if leg_positions:
            for leg_id, positions in leg_positions.items():
                self._render_leg(positions, leg_id)
            self._render_leg(positions, leg_id)
        
        # Flush to ensure all rendering is complete
        self.ctx.finish()
        
        # Return to default framebuffer
        self.ctx.screen.use()
    
    def _render_body(self, corners: List[np.ndarray]):
        """Render robot body as wireframe box"""
        if not corners or len(corners) < 8:
            return
        
        # Define edges of the box
        edges = [
            (0, 1), (1, 3), (3, 2), (2, 0),  # Top face
            (4, 5), (5, 7), (7, 6), (6, 4),  # Bottom face
            (0, 4), (1, 5), (2, 6), (3, 7),  # Vertical edges
        ]
        
        lines = []
        color = [0.8, 0.8, 0.8]
        
        for i, j in edges:
            lines.extend([*corners[i], *color])
            lines.extend([*corners[j], *color])
        
        if lines:
            data = np.array(lines, dtype='f4')
            vbo = self.ctx.buffer(data.tobytes())
            vao = self.ctx.vertex_array(
                self.line_program,
                [(vbo, '3f 3f', 'in_position', 'in_color')]
            )
            vao.render(moderngl.LINES)
            vao.release()
            vbo.release()
    
    def _render_leg(self, positions: List[np.ndarray], leg_id: int):
        """Render a single leg"""
        if len(positions) < 2:
            return
        
        # Different colors for different legs
        colors = [
            [1.0, 0.5, 0.5],  # Leg 0 - Red
            [0.5, 1.0, 0.5],  # Leg 1 - Green
            [0.5, 0.5, 1.0],  # Leg 2 - Blue
            [1.0, 1.0, 0.5],  # Leg 3 - Yellow
        ]
        color = colors[leg_id % 4]
        
        # Create line segments
        lines = []
        for i in range(len(positions) - 1):
            lines.extend([*positions[i], *color])
            lines.extend([*positions[i + 1], *color])
        
        # Create joints as small crosses
        joint_color = [1.0, 1.0, 1.0]
        joint_size = 5.0
        
        for pos in positions[1:]:  # Skip mounting point
            # Draw a small cross at each joint
            lines.extend([
                pos[0] - joint_size, pos[1], pos[2], *joint_color,
                pos[0] + joint_size, pos[1], pos[2], *joint_color,
            ])
            lines.extend([
                pos[0], pos[1] - joint_size, pos[2], *joint_color,
                pos[0], pos[1] + joint_size, pos[2], *joint_color,
            ])
            lines.extend([
                pos[0], pos[1], pos[2] - joint_size, *joint_color,
                pos[0], pos[1], pos[2] + joint_size, *joint_color,
            ])
        
        if lines:
            data = np.array(lines, dtype='f4')
            vbo = self.ctx.buffer(data.tobytes())
            vao = self.ctx.vertex_array(
                self.line_program,
                [(vbo, '3f 3f', 'in_position', 'in_color')]
            )
            vao.render(moderngl.LINES)
            vao.release()
            vbo.release()
    
    def get_texture_id(self) -> int:
        """Get OpenGL texture ID for ImGui"""
        return self.texture.glo
    
    def cleanup(self):
        """Release resources"""
        if self.grid_vao:
            self.grid_vao.release()
        if self.fbo:
            self.fbo.release()
        if self.texture:
            self.texture.release()
