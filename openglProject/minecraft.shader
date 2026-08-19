#VERTEX SHADER 
#version 330 core

layout(location = 0) in vec3 vPos; 
layout(location = 1) in vec3 vNormal; 
layout(location = 2) in float vBlockT; 

uniform mat4 projection; 
uniform mat4 view; 
uniform mat4 model; 

// Values passed from the vertex shader to the fragment shader.
out vec3 normal; 
out vec3 vertexPos; 
flat out float BlockType; 

void main(){ 

	
	vec4 worldPos = model * vec4(vPos, 1.0); // Transform the local vertex position into world space.
	
	mat3 normalMat = inverse(transpose(mat3(model))); // Inverse-transpose normal matrix keeps normals correct under model transformations.
	normal = normalize(normalMat * vNormal); 
	BlockType = vBlockT; 
	vertexPos = worldPos.xyz; 
	
	gl_Position = projection * view * worldPos; // Convert the world-space vertex into clip space for rasterization.
} 

#FRAGMENT SHADER 
#version 330 core

 
out vec4 fragColor; // Final color written by this fragment shader.
 
uniform vec3 camPos; // World-space camera position used to calculate fog distance.

// Convert the numeric block ID carried in the vertex data into its base RGB color.
vec3 getBlockColor(float blockT){ 
	if(blockT < 1.5){ 
		return vec3(0.52, 0.52, 0.52); // STONE 
	} 
	else if(blockT < 2.5){ 
		 return vec3(0.42, 0.27, 0.12); // WOOD 
	} 
	else if(blockT < 3.5){ 
		return vec3(0.30, 0.68, 0.22); // GRASS 
	} 
	else if(blockT < 4.5){ 
		return vec3(0.50, 0.32, 0.18); // DIRT 
	} 
	else if(blockT < 5.5){ 
		return vec3(0.10, 0.48, 0.10); // LEAVES 
	} 
	else if(blockT < 6.5){ 
		 return vec3(0.18, 0.38, 0.78); // WATER 
	} 
	else if(blockT < 7.5){ 
		return vec3(0.86, 0.77, 0.48); // SAND 
	} 

	//invalid blockType (return a purple block color) 
	return vec3(0.98, 0, 0.933); 
} 

// Interpolated values received from the vertex shader.
flat in float BlockType; 
in vec3 normal; 
in vec3 vertexPos; 

void main(){ 
	
	vec3 baseColor = getBlockColor(BlockType); // get the material color for this block type.

	vec3 norm = normalize(normal); // Renormalize because interpolated normals are not guaranteed to remain unit length.
	float faceBrightness; 

	// Apply fixed Minecraft-style brightness based on which axis the face normal points toward.
	if	  (abs(norm.z) > 0.9){ 
		faceBrightness =  0.68;   //front/back face 
	} 

	else if(abs(norm.x) > 0.9){ 
		faceBrightness =  0.82;  //right/left face 
	} 

	else if(norm.y > 0.9){ 
		faceBrightness =  1.00;	 //top face 
	}		 
	else if(norm.y < -0.9){ 
		faceBrightness =  0.48;	 //bottom face 
	} 

	vec3 lightDir = normalize(vec3(0.35, 1.0, 0.45)); // Unit vector representing the direction toward the light source.

	//alignment with lightDirection 
	float diffuse = max(dot(lightDir, norm), 0); 

	//minimum brightness 
	float ambient = 0.58; 

	// Combine constant ambient light with Lambert diffuse lighting.
	float lighting = ambient + 0.42 * diffuse; 
	lighting*= faceBrightness; 

	vec3 finalColor = baseColor * lighting; 
	// Get distance of fragment's world position to the camera.
	float cameraDistance = length(vertexPos - camPos); 

	vec3 fogColor = vec3(0.50, 0.75, 1.00); 
	float fogStart = 40.0; 
	float fogEnd = 80.0; 

	// Convert camera distance into a smooth 0..1 fog blend factor.
	float fogDistance = smoothstep(	fogStart,  
									fogEnd,  
									cameraDistance); 
	// Blend the lit block color toward the sky/fog color as distance increases.
	finalColor = mix( finalColor,  
					  fogColor,  
					  fogDistance); 

	// Output the final fragment color.
	fragColor = vec4(finalColor, 1.0); 


}