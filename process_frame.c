/* Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty. This file is offered as-is,
 * without any warranty.
 */

/*! @file process_frame.c
 * @brief Contains the actual algorithm and calculations.
 */

/* Definitions specific to this application. Also includes the Oscar main header file. */
#include "template.h"
#include <string.h>
#include <stdlib.h>

OSC_ERR OscVisDrawBoundingBoxBW(struct OSC_PICTURE *picIn, struct OSC_VIS_REGIONS *regions, uint8 Color);

void ProcessFrame(uint8 *pInputImg)
{
	int c, r;
	int nc = OSC_CAM_MAX_IMAGE_WIDTH/2;
	int siz = sizeof(data.u8TempImage[GRAYSCALE]);


	//int Shift = 7;
	//short Beta = 2;//the meaning is that in floating point the value of Beta is = 6/(1 << Shift) = 6/128 = 0.0469
	//uint8 MaxForeground = 120;//the maximum foreground counter value (at 15 fps this corresponds to less than 10s)

	struct OSC_PICTURE Pic1, Pic2;//we require these structures to use Oscar functions
	struct OSC_VIS_REGIONS ImgRegions;//these contain the foreground objects

	uint32 Hist[256];
	uint8 *p = data.u8TempImage[GRAYSCALE];
	uint32 W0[256];
	uint32 W1[256];
	uint32 M0[256];
	uint32 M1[256];
	double sigma2[256];
	uint32 k;
	uint32 g;
	uint32 thr;

	memset(Hist,0,sizeof(Hist));

	if(data.ipc.state.nStepCounter == 1)
	{
		memcpy(data.u8TempImage[BACKGROUND], data.u8TempImage[GRAYSCALE], sizeof(data.u8TempImage[GRAYSCALE]));
		memset(data.u8TempImage[FGRCOUNTER], 0, sizeof(data.u8TempImage[FGRCOUNTER]));
	}
	else
	{
			if(data.ipc.state.nThreshold != 0)
			{

				for(r = 0; r < siz; r+= nc)/* we strongly rely on the fact that them images have the same size */
				{
					for(c = 0; c < nc; c++)
					{
						/* first determine the foreground estimate */
						data.u8TempImage[THRESHOLD][r+c] = (short) data.u8TempImage[GRAYSCALE][r+c] > data.ipc.state.nThreshold ? 0 : 0xff;
					}
				}
			}
			else
			{
				/* this is the default case */
				for(r = 0; r < siz; r+= nc)/* we strongly rely on the fact that them images have the same size */
				{
					for(c = 0; c < nc; c++)
					{
						Hist[p[r +c]] += 1;
					}
				}

				thr=0;

				for (k=0; k <= 255; k++)
				{
					W0[k] = 0;
					M0[k] = 0;
					W1[k] = 0;
					M1[k] = 0;

					for (g=0; g<= k ; g++)
					{
						W0[k] += Hist[g];
						M0[k] += Hist[g] * g;
					}

					for (g=k+1; g<= 255 ; g++)
					{
						W1[k] += Hist[g];
						M1[k] += Hist[g] * g;
					}
					double term;
					if (W0[k] == 0 || W1[k] == 0)
					{
						term =  (double) M0[k] - (double) M1[k];
					}
					else{
						term = 1 / (double) W0[k] * (double) M0[k] - 1/(double) W1[k] * (double) M1[k];
					}
					sigma2[k] = (double) W0[k] * (double) W1[k] / 16777216  * term * term;

					if (sigma2[k] > sigma2[thr]){
						thr = k;
					}
				}

				thr =  thr / 255 * 100;

				//thr = 20; test unterer teil
				for(r = 0; r < siz; r+= nc)/* we strongly rely on the fact that them images have the same size */
				{
					for(c = 0; c < nc; c++)
					{
						/* first determine the foreground estimate */
						data.u8TempImage[THRESHOLD][r+c] = (short)data.u8TempImage[GRAYSCALE][r+c] > thr ? 0 : 0xff;
					}
				}
			}
		/*
		{
			//for debugging purposes we log the background values to console out
			//we chose the center pixel of the image (adaption to other pixel is straight forward)
			int offs = nc*(OSC_CAM_MAX_IMAGE_HEIGHT/2)/2+nc/2;

			OscLog(INFO, "%d %d %d %d %d\n", (int) data.u8TempImage[GRAYSCALE][offs], (int) data.u8TempImage[BACKGROUND][offs], (int) data.u8TempImage[BACKGROUND][offs]-data.ipc.state.nThreshold,
											 (int) data.u8TempImage[BACKGROUND][offs]+data.ipc.state.nThreshold, (int) data.u8TempImage[FGRCOUNTER][offs]);
		}
		*/


			//for debugging -> log Histogram
						for (k=0;k<=255;k++)
						{
							//log all
							OscLog(INFO, "[%d]\t %d\t %d\t %d\t %d\t %d\t %f\n",k,Hist[k],W0[k],M0[k],W1[k],M1[k],sigma2[k]);
							//log custom
							OscLog(INFO, "Hist[%d]\t %d\n", k , Hist[k]);
							OscLog(INFO, "W0[%d]\t %d\n", k , W0[k]);
							OscLog(INFO, "M0[%d]\t %d\n", k , M0[k]);
							OscLog(INFO, "W1[%d]\t %d\n", k , W1[k]);
							OscLog(INFO, "M1[%d]\t %d\n", k , M1[k]);
							OscLog(INFO, "ob2[%d]\t %d\n", k , sigma2[k]);

						}
						OscLog(INFO, "thr\t %d\n", thr);


		for(r = nc; r < siz-nc; r+= nc)/* we skip the first and last line */
		{
			for(c = 1; c < nc-1; c++)/* we skip the first and last column */
			{
				unsigned char* p = &data.u8TempImage[THRESHOLD][r+c];
				data.u8TempImage[DILATION][r+c] = *(p-nc-1) | *(p-nc) | *(p-nc+1) |
												  *(p-1)    | *p      | *(p+1)    |
												  *(p+nc-1) | *(p+nc) | *(p+nc+1);
			}
		}

		for(r = nc; r < siz-nc; r+= nc)/* we skip the first and last line */
		{
			for(c = 1; c < nc-1; c++)/* we skip the first and last column */
			{
				unsigned char* p = &data.u8TempImage[DILATION][r+c];
				data.u8TempImage[EROSION][r+c] = *(p-nc-1) & *(p-nc) & *(p-nc+1) &
												 *(p-1)    & *p      & *(p+1)    &
												 *(p+nc-1) & *(p+nc) & *(p+nc+1);
			}
		}



		//wrap image DILATION in picture struct
		Pic1.data = data.u8TempImage[DILATION];
		Pic1.width = nc;
		Pic1.height = OSC_CAM_MAX_IMAGE_HEIGHT/2;
		Pic1.type = OSC_PICTURE_GREYSCALE;
		//as well as EROSION (will be used as output)
		Pic2.data = data.u8TempImage[EROSION];
		Pic2.width = nc;
		Pic2.height = OSC_CAM_MAX_IMAGE_HEIGHT/2;
		Pic2.type = OSC_PICTURE_BINARY;//probably has no consequences
		//have to convert to OSC_PICTURE_BINARY which has values 0x01 (and not 0xff)
		OscVisGrey2BW(&Pic1, &Pic2, 0x80, false);

		//now do region labeling and feature extraction
		OscVisLabelBinary( &Pic2, &ImgRegions);
		OscVisGetRegionProperties( &ImgRegions);

		//OscLog(INFO, "number of objects %d\n", ImgRegions.noOfObjects);
		//plot bounding boxes both in gray and dilation image
		Pic2.data = data.u8TempImage[GRAYSCALE];
		OscVisDrawBoundingBoxBW( &Pic2, &ImgRegions, 255);
		OscVisDrawBoundingBoxBW( &Pic1, &ImgRegions, 128);
	}
}


/* Drawing Function for Bounding Boxes; own implementation because Oscar only allows colored boxes; here in Gray value "Color"  */
/* should only be used for debugging purposes because we should not drawn into a gray scale image */
OSC_ERR OscVisDrawBoundingBoxBW(struct OSC_PICTURE *picIn, struct OSC_VIS_REGIONS *regions, uint8 Color)
{
	 uint16 i, o;
	 uint8 *pImg = (uint8*)picIn->data;
	 const uint16 width = picIn->width;
	 for(o = 0; o < regions->noOfObjects; o++)//loop over regions
	 {
		 /* Draw the horizontal lines. */
		 for (i = regions->objects[o].bboxLeft; i < regions->objects[o].bboxRight; i += 1)
		 {
				 pImg[width * regions->objects[o].bboxTop + i] = Color;
				 pImg[width * (regions->objects[o].bboxBottom - 1) + i] = Color;
		 }

		 /* Draw the vertical lines. */
		 for (i = regions->objects[o].bboxTop; i < regions->objects[o].bboxBottom-1; i += 1)
		 {
				 pImg[width * i + regions->objects[o].bboxLeft] = Color;
				 pImg[width * i + regions->objects[o].bboxRight] = Color;
		 }
	 }
	 return SUCCESS;

}


