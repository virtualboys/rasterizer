
// column major
float4 vec4_mul_mat4(const float4 vec, global const float4* mat) {
	return (float4)(vec.x * mat[0][0] + vec.y * mat[1][0] + vec.z * mat[2][0] + vec.w * mat[3][0],
					vec.x * mat[0][1] + vec.y * mat[1][1] + vec.z * mat[2][1] + vec.w * mat[3][1],
					vec.x * mat[0][2] + vec.y * mat[1][2] + vec.z * mat[2][2] + vec.w * mat[3][2],
					vec.x * mat[0][3] + vec.y * mat[1][3] + vec.z * mat[2][3] + vec.w * mat[3][3]);
}

float3 barycentric(float2 A, float2 B, float2 C, float2 P, float offset) {
    float3 s[2];
    for (int i=2; i--; ) {
        s[i][0] = C[i]-A[i];
        s[i][1] = B[i]-A[i];
        s[i][2] = A[i]-P[i];
    }
    float3 u = cross(s[0], s[1]);
    if (fabs(u[2])>1e-2) // 
        //dont forget that u[2] is integer. If it is zero then triangle ABC is degenerate
        return (float3)(1.f-(u.x+u.y * offset)/u.z, u.y/u.z, offset * u.x/u.z);
    return (float3)(-1,1,1); // in this case generate negative coordinates, it will be thrown away by the rasterizator
}

float3 interpolateBary3(float3 a, float3 b, float3 c, float3 bar) {
	return (float3)(a.x * bar.x + b.x * bar.y + c.x * bar.z, 
					a.y * bar.x + b.y * bar.y + c.y * bar.z, 
					a.z * bar.x + b.z * bar.y + c.z * bar.z);
}

float2 interpolateBary2(float2 a, float2 b, float2 c, float3 bar) {
	return (float2)(a.x * bar.x + b.x * bar.y + c.x * bar.z, 
					a.y * bar.x + b.y * bar.y + c.y * bar.z);
}

void printVec(float4 vec) {
	printf("vec: %f %f %f %f\n", vec.x, vec.y, vec.z, vec.w);
}

kernel void rasterizer(
		ulong nTris,
		int width,
		int height,
		float offset,
		global float* zbuffer,
		global const float4* viewport, 
		global const float* vertices,
		global const float* normalsIn,
		global const float* uvsIn,
		global const int* indices,
		global char* screen,
		global float* normalsOut,
		global float* uvsOut)
{
	size_t faceInd = get_global_id(0);
	offset = 1 + offset;

	float3 homoZs;
	float4 vsVP[3];
	float3 vs[3];
	float3 ns[3];
	float2 uvs[3];
	for(int i = 0; i < 3; i++) {
		// indices are vertex/uv/normal
		int vertInd = indices[faceInd*9+i*3];
		//int uvInd = indices[faceInd*9+i*3+1];
		//int normalInd = indices[faceInd*9+i*3+2];
		// normalInd = vertInd;
		// uv
		
		float4 vertHomo = (float4)(vertices[vertInd*4], vertices[vertInd*4+1], vertices[vertInd*4+2], vertices[vertInd*4+3]);
		
		homoZs[i] = vertHomo.z;
		vertHomo = vec4_mul_mat4(vertHomo, viewport);
		vsVP[i] = vertHomo;
		vs[i] = vertHomo.xyz / vertHomo.w;

		ns[i] = (float3)(normalsIn[vertInd*3], normalsIn[vertInd*3+1], normalsIn[vertInd*3+2]);
		uvs[i] = (float2)(uvsIn[vertInd * 2], uvsIn[vertInd * 2 + 1]);
	}

	float2 bboxmin = (float2)(INFINITY,INFINITY);
	float2 bboxmax = (float2)(-INFINITY,-INFINITY);

	float2 clampCoords = (float2)(width-1, height-1);

	for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            bboxmin[j] = max(0.f, min(bboxmin[j], vs[i][j]));
            bboxmax[j] = min(clampCoords[j], max(bboxmax[j], vs[i][j]));
        }
    }

    int2 pix;
    for (pix.x=bboxmin.x; pix.x<=bboxmax.x; pix.x++) {
        for (pix.y=bboxmin.y; pix.y<=bboxmax.y; pix.y++) {
            float3 bc_screen  = barycentric(vs[0].xy, vs[1].xy, vs[2].xy, (float2)(pix.x,pix.y), offset);
            float3 bc_clip    = (float3)(bc_screen.x/vsVP[0][3], bc_screen.y/vsVP[1][3], bc_screen.z/vsVP[2][3]);
            //printf("bccl: %f %f %f\n", bc_clip.x, bc_clip.y, bc_clip.z);

            bc_clip = bc_clip/(bc_clip.x+bc_clip.y+bc_clip.z);

    		
            float frag_depth = dot(homoZs, bc_clip);// clipc[2]*bc_clip;
            //printf("frag: %f\n", frag_depth);
            int pixInd = pix.x+pix.y*width;

            if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0 || zbuffer[pixInd]>frag_depth) continue;

            zbuffer[pixInd] = frag_depth;
            zbuffer[(int)(pixInd * (offset))] = frag_depth;

            screen[pixInd*3] = (.5f + .5f * bc_screen.x) * 255;
            screen[pixInd*3+1] = (.5f + .5f * bc_screen.y) * 255;
            screen[pixInd*3+2] = (.5f + .5f * bc_screen.z) * 255;

            float3 n = interpolateBary3(ns[0], ns[1], ns[2], bc_clip);
            normalsOut[pixInd*3] = n.x;
            normalsOut[pixInd*3+1] = n.y;
            normalsOut[pixInd*3+2] = n.z;

            float2 uv = interpolateBary2(uvs[0], uvs[1], uvs[2], bc_clip);
            uvsOut[pixInd * 2] = uv.x;
            uvsOut[pixInd * 2 + 1] = uv.y;
        	// printf("uv: %f %f", uv.x, uv.y);

        }
    }
}

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

kernel void fragment(
		int width,
		int height,
		float offset,
		float3 viewDir,
		global const float* zBuffer,
		global const float* normals,
		global const float* uvs,
		image2d_t tex,
		global char* color
	)
{
	size_t i = get_global_id(0);

	// printf("zbuf: %f", zBuffer[i]);
	if(zBuffer[i] > .1f) {
		int x = i % width;
		int y = i / width;

		float3 n = (float3)(normals[i*3], normals[i*3+1], normals[i*3+2]);
		float2 uv = (float2)(uvs[i*2], uvs[i*2+1]);
    	// printf("uv: %f %f", uv.x, uv.y);

    	float4 col = read_imagef(tex, sampler, uv * (1+offset ) *cos(uv.x * (1+offset)));
    	

		float val = dot(n, viewDir);
		// col = (float4)(val,val,val,1);
		col *= val;
		// color[i*3] = (char)(n.x * 255);
		// color[i*3+1] = (char)(n.y * 255);
		// color[i*3+2] = (char)(n.z * 255);	
		color[i*3] = (char)(col.x * 255);
		color[i*3+1] = (char)(col.y * 255);
		color[i*3+2] = (char)(col.z * 255);	
	}
}

kernel void vertex(
		ulong n,
		float offset,
		global const float4* mvp, 
		global const float4* modelMat,
		global const float* vertices,
		global const float* normalsIn,
		global float* positions,
		global float* normalsOut
	)
{
	size_t i = get_global_id(0);

	float4 vert = (float4)(vertices[i*3], vertices[i*3+1], vertices[i*3+2], 1.0f);
	float4 normal = (float4)(normalsIn[i*3], normalsIn[i*3+1], normalsIn[i*3+2],1.0f);

	

	normal = vec4_mul_mat4(normal, modelMat);
	normalize(normal);

	vert = vec4_mul_mat4(vert, mvp);

	vert.xy += (float2)(cos(100 *vert.x), sin(100 * vert.y)) * offset;

	positions[i*4] = vert.x;
	positions[i*4+1] = vert.y;
	positions[i*4+2] = vert.z;
	positions[i*4+3] = vert.w;

	

	normalsOut[i*3] = normal.x;
	normalsOut[i*3+1] = normal.y;
	normalsOut[i*3+2] = normal.z;
}
