
// column major
float4 vec4_mul_mat4(const float4 vec, global const float4* mat) {
	return (float4)(vec.x * mat[0][0] + vec.y * mat[1][0] + vec.z * mat[2][0] + vec.w * mat[3][0],
					vec.x * mat[0][1] + vec.y * mat[1][1] + vec.z * mat[2][1] + vec.w * mat[3][1],
					vec.x * mat[0][2] + vec.y * mat[1][2] + vec.z * mat[2][2] + vec.w * mat[3][2],
					vec.x * mat[0][3] + vec.y * mat[1][3] + vec.z * mat[2][3] + vec.w * mat[3][3]);
}

float3 barycentric(float2 A, float2 B, float2 C, float2 P, float offset) {
    float3 s[2];
    A*= offset;

    for (int i=2; i--; ) {
        s[i][0] = C[i]-(A[i]);
        s[i][1] = B[i]-(A[i]);
        s[i][2] = A[i]-P[i];
    }
    float3 u = cross(s[0], s[1]);
    if (fabs(u[2])>1e-2) // 
        //dont forget that u[2] is integer. If it is zero then triangle ABC is degenerate
        return (float3)(1.f-(u.x+u.y)/u.z, u.y/u.z, u.x/u.z);
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
	//printf("vec: %f %f %f %f\n", vec.x, vec.y, vec.z, vec.w);
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
	// printf("vertex");
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

// this kernel is executed once per polygon
// it computes which tiles are occupied by the polygon and adds the index of the polygon to the list for that tile
kernel void tiler(
        // number of polygons
		ulong nTris,
        // width of screen
		int width,
        // height of screen
		int height,
        // number of tiles in x direction
		int tilesX,
        // number of tiles in y direction
		int tilesY,
        // number of pixels per tile (tiles are square)
		int tileSize,
        // size of the polygon list for each tile
		int polysPerTile,
        // 4x4 matrix representing the viewport
		global const float4* viewport, 
        // vertex positions
		global const float* vertices,
        // indices of vertices
		global const int* indices,
        // array of array-lists of polygons per tile
        // structure of list is an int representing the number of polygons covering that tile, 
        // followed by [polysPerTile] integers representing the indices of the polygons in that tile
        // there are [tilesX*tilesY] such arraylists
		volatile global int* tilePolys)
{
	size_t faceInd = get_global_id(0);

    // compute vertex position in viewport space
	float3 vs[3];
	float4 vsVP[3];

	for(int i = 0; i < 3; i++) {
		// indices are vertex/uv/normal
		int vertInd = indices[faceInd * 9 + i * 3] * 4;
		
		float4 vertHomo = (float4)(vertices[vertInd], vertices[vertInd + 1], vertices[vertInd + 2], vertices[vertInd + 3]);
		
		vertHomo = vec4_mul_mat4(vertHomo, viewport);

		vsVP[i] = vertHomo;
		vs[i] = vertHomo.xyz / vertHomo.w;
	}

	float2 bboxmin = (float2)(INFINITY,INFINITY);
	float2 bboxmax = (float2)(-INFINITY,-INFINITY);

    // size of screen
	float2 clampCoords = (float2)(width-1, height-1);

    // compute bounding box of triangle in screen space
	for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            bboxmin[j] = max(0.f, min(bboxmin[j], vs[i][j]));
            bboxmax[j] = min(clampCoords[j], max(bboxmax[j], vs[i][j]));
        }
    }

    // transform bounding box to tile space
    int2 tilebboxmin = (int2)(bboxmin[0] / tileSize, bboxmin[1] / tileSize);
    int2 tilebboxmax = (int2)(bboxmax[0] / tileSize, bboxmax[1] / tileSize);

    // loop over all tiles in bounding box
    for(int x = tilebboxmin[0]; x <= tilebboxmax[0]; x++) {
    	for(int y = tilebboxmin[1]; y <= tilebboxmax[1]; y++) {

    			float2 pix = (float2)(x * tileSize, y * tileSize);
	            float3 bc_screen  = barycentric(vs[0].xy, vs[1].xy, vs[2].xy, (float2)(pix.x,pix.y), 1);
	            // float3 bc_clip    = (float3)(bc_screen.x/vsVP[0][3], bc_screen.y/vsVP[1][3], bc_screen.z/vsVP[2][3]);

	            // bc_clip = bc_clip/(bc_clip.x+bc_clip.y+bc_clip.z);

	            // if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0) continue;

            // get index of tile
    		int tileInd = y * tilesX + x;
            // get start index of polygon list for this tile
    		int counterInd = tileInd * (polysPerTile + 1);
            // get current number of polygons in list
    		int numPolys = atomic_inc(&tilePolys[counterInd]);
            // if list is full, skip tile
    		if(numPolys >= polysPerTile) {
    			atomic_dec(&tilePolys[counterInd]);
    		} else {
                // otherwise add the poly to the list
                // the index is the offset + numPolys + 1 as tilePolys[counterInd] holds the poly count
    			int ind = counterInd + numPolys + 1;
    			tilePolys[ind] = (int)(faceInd);
    		}	
    	}
    }
}

kernel void tileRasterizer(
		ulong nTris,
		int width,
		int height,
		int tilesX,
		int tilesY,
		int tileSize,
		int polysPerTile,
		float offset,
		global float* zbuffer,
		global const float4* viewport, 
		global const float* vertices,
		global const float* normalsIn,
		global const float* uvsIn,
		global const int* indices,
		global int* tilePolys,
		global char* screen,
		global float* normalsOut,
		global float* uvsOut)
{
	// printf("tilerast");
	size_t tileInd = get_global_id(0);
	offset = 1 + offset;

	float2 tileMin = (float2)(tileSize * (tileInd % tilesX), tileSize * (tileInd / tilesX));
	float2 tileMax = (float2)(tileMin.x + tileSize, tileMin.y + tileSize);

	int polyListOffset = tileInd * (polysPerTile + 1);
	int numPolys = tilePolys[polyListOffset];

	// if(numPolys > 200) {
		// printf("offset: %d tile: %d x %d y %d polyCount %d \n", polyListOffset, tileInd, tileInd % tilesX, tileInd / tilesX, numPolys); 
	// }
	numPolys = min(numPolys, polysPerTile);

	for(int ind = 0; ind < numPolys; ind++) {

		int faceInd = tilePolys[polyListOffset + ind + 1];
	// printf("tile: %d polyCount %d faceInd %d\n", polyListOffset, numPolys, 0); 

		float3 homoZs;
		float4 vsVP[3];
		float3 vs[3];
		float3 ns[3];
		float2 uvs[3];
		for(int i = 0; i < 3; i++) {
			// indices are vertex/uv/normal
			int vertInd = indices[faceInd*9+i*3];
			
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

		for (int i=0; i<3; i++) {
	        for (int j=0; j<2; j++) {
	            bboxmin[j] = max(tileMin[j], min(bboxmin[j], vs[i][j]));
	            bboxmax[j] = min(tileMax[j], max(bboxmax[j], vs[i][j]));
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


	            // this maybe should be atomic!
	            if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0 || zbuffer[pixInd]<frag_depth) continue;

	            zbuffer[pixInd] = frag_depth;

	            int offsetInd = (int)(pixInd * offset) % (width * height);
	            offsetInd = max(0,offsetInd);
	            // zbuffer[offsetInd] = frag_depth;

	            int offsetPixIndX = (int)( ( fabs((float)(.1f * offset)) * pix.x));
	            int offsetPixIndY = (int)( (fabs((float)(.1f * offset)) * pix.y));
	            int offsetPixInd = offsetPixIndX+offsetPixIndY*width;
	            // screen[offsetPixInd*3] = (.5f + .5f * bc_screen.x) * 255;
	            // screen[offsetPixInd*3+1] = (.5f + .5f * bc_screen.y) * 255;
	            // screen[offsetPixInd*3+2] = (.5f + .5f * bc_screen.z) * 255;


	            // screen[pixInd*3] = (.5f + .5f * bc_screen.x) * 255;
	            // screen[pixInd*3+1] = (.5f + .5f * bc_screen.y) * 255;
	            // screen[pixInd*3+2] = (.5f + .5f * bc_screen.z) * 255;

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
}

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

kernel void fragment(
		int width,
		int height,
		float offset,
		// float3 viewDir,
		global const float* zBuffer,
		global const float* normals,
		global const float* uvs,
		image2d_t tex,
		global char* color
	)
{
	// printf("fragment");
	size_t i = get_global_id(0);

	// printf("zbuf: %f", zBuffer[i]);
	//if(zBuffer[i] > .1f) {
	{
		int x = i % width;
		int y = i / width;

		float3 n = (float3)(normals[i*3], normals[i*3+1], normals[i*3+2]);
		float2 uv = (float2)(uvs[i*2], uvs[i*2+1]);
    	// printf("uv: %f %f", uv.x, uv.y);

		// float4 col = (float4)(1,1,1,1);
		float4 col = (float4)(uv.x,uv.y,1,1);
    	// float4 col = read_imagef(tex, sampler, uv);
    	

		// float val = dot(n, viewDir);
		// col = (float4)(val,val,val,1);
		// col.r *= val;
		col.g = n.y;
		// col.g *= val;
		// color[i*3] = (char)(n.x * 255);
		// color[i*3+1] = (char)(n.y * 255);
		// color[i*3+2] = (char)(n.z * 255);	
		color[i*3] = (char)(col.x * 255);
		color[i*3+1] = (char)(col.y * 255);
		color[i*3+2] = (char)(col.z * 255);	
	}
}

// kernel void rasterizer(
// 		ulong nTris,
// 		int width,
// 		int height,
// 		float offset,
// 		global float* zbuffer,
// 		global const float4* viewport, 
// 		global const float* vertices,
// 		global const float* normalsIn,
// 		global const float* uvsIn,
// 		global const int* indices,
// 		global char* screen,
// 		global float* normalsOut,
// 		global float* uvsOut)
// {
// 	// printf("rast");
// 	size_t faceInd = get_global_id(0);
// 	offset = 1 + offset;

// 	float3 homoZs;
// 	float4 vsVP[3];
// 	float3 vs[3];
// 	float3 ns[3];
// 	float2 uvs[3];
// 	for(int i = 0; i < 3; i++) {
// 		// indices are vertex/uv/normal
// 		int vertInd = indices[faceInd*9+i*3];
// 		//int uvInd = indices[faceInd*9+i*3+1];
// 		//int normalInd = indices[faceInd*9+i*3+2];
// 		// normalInd = vertInd;
// 		// uv
		
// 		float4 vertHomo = (float4)(vertices[vertInd*4], vertices[vertInd*4+1], vertices[vertInd*4+2], vertices[vertInd*4+3]);
		
// 		homoZs[i] = vertHomo.z;
// 		vertHomo = vec4_mul_mat4(vertHomo, viewport);
// 		vsVP[i] = vertHomo;
// 		vs[i] = vertHomo.xyz / vertHomo.w;

// 		ns[i] = (float3)(normalsIn[vertInd*3], normalsIn[vertInd*3+1], normalsIn[vertInd*3+2]);
// 		uvs[i] = (float2)(uvsIn[vertInd * 2], uvsIn[vertInd * 2 + 1]);
// 	}

// 	float2 bboxmin = (float2)(INFINITY,INFINITY);
// 	float2 bboxmax = (float2)(-INFINITY,-INFINITY);

// 	float2 clampCoords = (float2)(width-1, height-1);

// 	for (int i=0; i<3; i++) {
//         for (int j=0; j<2; j++) {
//             bboxmin[j] = max(0.f, min(bboxmin[j], vs[i][j]));
//             bboxmax[j] = min(clampCoords[j], max(bboxmax[j], vs[i][j]));
//         }
//     }

//     int2 pix;
//     for (pix.x=bboxmin.x; pix.x<=bboxmax.x; pix.x++) {
//         for (pix.y=bboxmin.y; pix.y<=bboxmax.y; pix.y++) {
//             float3 bc_screen  = barycentric(vs[0].xy, vs[1].xy, vs[2].xy, (float2)(pix.x,pix.y), offset);
//             float3 bc_clip    = (float3)(bc_screen.x/vsVP[0][3], bc_screen.y/vsVP[1][3], bc_screen.z/vsVP[2][3]);
//             //printf("bccl: %f %f %f\n", bc_clip.x, bc_clip.y, bc_clip.z);

//             bc_clip = bc_clip/(bc_clip.x+bc_clip.y+bc_clip.z);

    		
//             float frag_depth = dot(homoZs, bc_clip);// clipc[2]*bc_clip;
//             //printf("frag: %f\n", frag_depth);
//             int pixInd = pix.x+pix.y*width;

//             if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0 || zbuffer[pixInd]<frag_depth) continue;

//             zbuffer[pixInd] = frag_depth;
//             zbuffer[(int)(pixInd * (offset))] = frag_depth;

//             screen[pixInd*3] = (.5f + .5f * bc_screen.x) * 255;
//             screen[pixInd*3+1] = (.5f + .5f * bc_screen.y) * 255;
//             screen[pixInd*3*50+2] = (.5f + .5f * bc_screen.z) * 255;

//             float3 n = interpolateBary3(ns[0], ns[1], ns[2], bc_clip);
//             normalsOut[pixInd*3] = n.x;
//             normalsOut[pixInd*3+1] = n.y;
//             normalsOut[pixInd*3+2] = n.z;

//             float2 uv = interpolateBary2(uvs[0], uvs[1], uvs[2], bc_clip);
//             uvsOut[pixInd * 2] = uv.x;
//             uvsOut[pixInd * 2 + 1] = uv.y;
//         	// printf("uv: %f %f", uv.x, uv.y);

//         }
//     }
// }
