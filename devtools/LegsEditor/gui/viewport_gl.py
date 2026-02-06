"""
3D Viewport rendering using PyOpenGL (compatible with imgui_bundle)
"""
import numpy as np
from OpenGL.GL import *
from OpenGL.GL.shaders import compileProgram, compileShader
import glm


class Camera:
    """3D Camera with orbital controls"""
    
    def __init__(self):
        self.distance = 500.0  # Distance from target
        self.azimuth = 45.0    # Horizontal angle (degrees)
        self.elevation = 30.0  # Vertical angle (degrees)
        self.target = glm.vec3(0, 0, 0)  # Look-at target
        
    def get_view_matrix(self):
        """Calculate view matrix from orbital parameters"""
        # Convert to radians
        az_rad = glm.radians(self.azimuth)
        el_rad = glm.radians(self.elevation)
        
        # Calculate camera position in spherical coordinates
        x = self.distance * glm.cos(el_rad) * glm.cos(az_rad)
        y = self.distance * glm.sin(el_rad)
        z = self.distance * glm.cos(el_rad) * glm.sin(az_rad)
        
        eye = self.target + glm.vec3(x, y, z)
        up = glm.vec3(0, 1, 0)
        
        return glm.lookAt(eye, self.target, up)
    
    def get_projection_matrix(self, aspect_ratio):
        """Calculate projection matrix"""
        fov = glm.radians(45.0)
        near = 1.0
        far = 10000.0
        return glm.perspective(fov, aspect_ratio, near, far)
    
    def orbit(self, delta_azimuth, delta_elevation):
        """Rotate camera around target"""
        self.azimuth += delta_azimuth
        self.elevation += delta_elevation
        
        # Clamp elevation to avoid gimbal lock
        self.elevation = max(-89.0, min(89.0, self.elevation))
        
        # Wrap azimuth
        while self.azimuth > 360.0:
            self.azimuth -= 360.0
        while self.azimuth < 0.0:
            self.azimuth += 360.0
    
    def zoom(self, delta):
        """Zoom camera in/out"""
        self.distance -= delta * 20.0
        self.distance = max(50.0, min(2000.0, self.distance))
    
    def pan(self, delta_x, delta_y):
        """Pan camera target"""
        import math
        
        # Convert angles to radians
        az_rad = math.radians(self.azimuth)
        
        # Calculate right and up vectors
        right_x = math.cos(az_rad + math.pi / 2)
        right_z = math.sin(az_rad + math.pi / 2)
        
        # Pan speed based on distance
        speed = self.distance * 0.001
        
        # Update target
        self.target.x += (right_x * delta_x - math.cos(az_rad) * delta_y) * speed
        self.target.z += (right_z * delta_x - math.sin(az_rad) * delta_y) * speed
    
    def snap_to_view(self, view_name):
        """Snap camera to predefined view"""
        views = {
            "front": (0.0, 0.0),
            "back": (180.0, 0.0),
            "left": (-90.0, 0.0),
            "right": (90.0, 0.0),
            "top": (0.0, 89.0),
            "bottom": (0.0, -89.0),
        }
        
        if view_name in views:
            self.azimuth, self.elevation = views[view_name]


class Viewport:
    """3D Viewport using raw OpenGL"""
    
    def __init__(self):
        self.width = 800
        self.height = 600
        self.camera = Camera()
        
        # OpenGL objects
        self.fbo = None
        self.texture = None
        self.depth_buffer = None
        self.shader_program = None
        self.vao = None
        self.vbo = None
        
        self._init_opengl()
        
    def _init_opengl(self):
        """Initialize OpenGL resources"""
        # Vertex shader
        vertex_shader = """
        #version 330 core
        layout(location = 0) in vec3 position;
        layout(location = 1) in vec3 color;
        
        uniform mat4 mvp;
        
        out vec3 fragColor;
        
        void main() {
            gl_Position = mvp * vec4(position, 1.0);
            fragColor = color;
        }
        """
        
        # Fragment shader
        fragment_shader = """
        #version 330 core
        in vec3 fragColor;
        out vec4 outColor;
        
        void main() {
            outColor = vec4(fragColor, 1.0);
        }
        """
        
        # Compile shaders
        self.shader_program = compileProgram(
            compileShader(vertex_shader, GL_VERTEX_SHADER),
            compileShader(fragment_shader, GL_FRAGMENT_SHADER)
        )
        
        # Create framebuffer
        self._create_framebuffer()
        
        print(f"Viewport GL initialized: {self.width}x{self.height}")
        print(f"Texture ID: {self.texture}, FBO: {self.fbo}")
        
    def _create_framebuffer(self):
        """Create framebuffer for off-screen rendering"""
        # Delete old resources if they exist
        if self.fbo:
            glDeleteFramebuffers(1, [self.fbo])
        if self.texture:
            glDeleteTextures([self.texture])
        if self.depth_buffer:
            glDeleteRenderbuffers(1, [self.depth_buffer])
            
        # Create texture for color attachment
        self.texture = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, self.texture)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, self.width, self.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, None)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        
        # Create depth renderbuffer
        self.depth_buffer = glGenRenderbuffers(1)
        glBindRenderbuffer(GL_RENDERBUFFER, self.depth_buffer)
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, self.width, self.height)
        
        # Create framebuffer
        self.fbo = glGenFramebuffers(1)
        glBindFramebuffer(GL_FRAMEBUFFER, self.fbo)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, self.texture, 0)
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, self.depth_buffer)
        
        # Check framebuffer status
        status = glCheckFramebufferStatus(GL_FRAMEBUFFER)
        if status != GL_FRAMEBUFFER_COMPLETE:
            print(f"Framebuffer incomplete: {status}")
        
        # Unbind
        glBindFramebuffer(GL_FRAMEBUFFER, 0)
        
    def resize(self, width, height):
        """Resize viewport"""
        if width != self.width or height != self.height:
            self.width = width
            self.height = height
            self._create_framebuffer()
            
    def render(self, leg_positions, body_corners):
        """Render the 3D scene"""
        # Bind our framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, self.fbo)
        
        # Set viewport
        glViewport(0, 0, self.width, self.height)
        
        # Clear
        glClearColor(0.2, 0.2, 0.25, 1.0)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        
        # Enable depth testing
        glEnable(GL_DEPTH_TEST)
        
        # Calculate MVP matrix
        view = self.camera.get_view_matrix()
        proj = self.camera.get_projection_matrix(self.width / self.height if self.height > 0 else 1.0)
        mvp = proj * view
        
        # Use shader
        glUseProgram(self.shader_program)
        mvp_loc = glGetUniformLocation(self.shader_program, "mvp")
        glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm.value_ptr(mvp))
        
        # Render grid
        self._render_grid()
        
        # Render body
        if body_corners:
            self._render_body(body_corners)
        
        # Render legs
        if leg_positions:
            self._render_legs(leg_positions)
        
        # Unbind framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0)
        
    def _render_grid(self):
        """Render ground grid"""
        grid_size = 500
        grid_step = 50
        
        vertices = []
        for i in range(-grid_size, grid_size + grid_step, grid_step):
            # Lines parallel to X axis
            vertices.extend([
                i, 0, -grid_size,  0.3, 0.3, 0.3,
                i, 0, grid_size,   0.3, 0.3, 0.3,
            ])
            # Lines parallel to Z axis
            vertices.extend([
                -grid_size, 0, i,  0.3, 0.3, 0.3,
                grid_size, 0, i,   0.3, 0.3, 0.3,
            ])
        
        vertices = np.array(vertices, dtype=np.float32)
        
        # Create VAO/VBO  
        vao = glGenVertexArrays(1)
        vbo = glGenBuffers(1)
        
        try:
            glBindVertexArray(vao)
            glBindBuffer(GL_ARRAY_BUFFER, vbo)
            glBufferData(GL_ARRAY_BUFFER, vertices.nbytes, vertices, GL_STATIC_DRAW)
            
            # Position attribute
            glEnableVertexAttribArray(0)
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, None)
            
            # Color attribute
            glEnableVertexAttribArray(1)
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, ctypes.c_void_p(12))
            
            # Draw
            glDrawArrays(GL_LINES, 0, len(vertices) // 6)
        finally:
            # Cleanup
            glDeleteVertexArrays(1, [vao])
            glDeleteBuffers(1, [vbo])
        
    def _render_body(self, corners):
        """Render robot body"""
        # Define body edges (cube)
        edges = [
            (0, 1), (1, 2), (2, 3), (3, 0),  # Bottom face
            (4, 5), (5, 6), (6, 7), (7, 4),  # Top face
            (0, 4), (1, 5), (2, 6), (3, 7),  # Vertical edges
        ]
        
        vertices = []
        for i, j in edges:
            c1 = corners[i]
            c2 = corners[j]
            # Add two vertices with cyan color
            vertices.extend([c1[0], c1[1], c1[2], 0.0, 1.0, 1.0])
            vertices.extend([c2[0], c2[1], c2[2], 0.0, 1.0, 1.0])
        
        vertices = np.array(vertices, dtype=np.float32)
        
        # Create VAO/VBO
        vao = glGenVertexArrays(1)
        vbo = glGenBuffers(1)
        
        try:
            glBindVertexArray(vao)
            glBindBuffer(GL_ARRAY_BUFFER, vbo)
            glBufferData(GL_ARRAY_BUFFER, vertices.nbytes, vertices, GL_STATIC_DRAW)
            
            # Position attribute
            glEnableVertexAttribArray(0)
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, None)
            
            # Color attribute
            glEnableVertexAttribArray(1)
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, ctypes.c_void_p(12))
            
            # Draw
            glDrawArrays(GL_LINES, 0, len(vertices) // 6)
        finally:
            # Cleanup
            glDeleteVertexArrays(1, [vao])
            glDeleteBuffers(1, [vbo])
        
    def _render_legs(self, leg_positions):
        """Render robot legs"""
        for leg_id, positions in leg_positions.items():
            if not positions or len(positions) < 2:
                continue
                
            vertices = []
            # Color gradient from yellow (base) to red (tip)
            num_points = len(positions)
            for i in range(num_points):
                p = positions[i]
                t = i / (num_points - 1) if num_points > 1 else 0
                r = 1.0
                g = 1.0 - t * 0.5  # Yellow to orange/red
                b = 0.0
                
                vertices.extend([p[0], p[1], p[2], r, g, b])
            
            # Draw as line strip
            vertices = np.array(vertices, dtype=np.float32)
            
            # Create VAO/VBO
            vao = glGenVertexArrays(1)
            vbo = glGenBuffers(1)
            
            try:
                glBindVertexArray(vao)
                glBindBuffer(GL_ARRAY_BUFFER, vbo)
                glBufferData(GL_ARRAY_BUFFER, vertices.nbytes, vertices, GL_STATIC_DRAW)
                
                # Position attribute
                glEnableVertexAttribArray(0)
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, None)
                
                # Color attribute
                glEnableVertexAttribArray(1)
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, ctypes.c_void_p(12))
                
                # Draw
                glLineWidth(1.0)  # Core profile only supports 1.0
                glDrawArrays(GL_LINE_STRIP, 0, len(vertices) // 6)
            finally:
                # Cleanup
                glDeleteVertexArrays(1, [vao])
                glDeleteBuffers(1, [vbo])
    
    def get_texture_id(self):
        """Get the OpenGL texture ID"""
        return self.texture
