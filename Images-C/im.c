#include <windows.h>
#include <stdio.h>
#include <math.h>

typedef struct tagRGBTriplet
{
	BYTE red;
	BYTE green;
	BYTE blue;
} RGBTriplet;


BYTE* BMPToTriplets(BYTE* bmp_data, int width, int height)
{
	int padding = 0;
	int scanlinebytes = width * 3;
	while ((scanlinebytes + padding) % 4 != 0)
		padding++;
	int padded_width = scanlinebytes + padding;
	BYTE* triplet_arr = malloc(width*height * 3);

	long i_bmp = 0;
	long i_triplet = 0;
	for (int y = 0; y < height; y++)
		for (int x = 0; x < 3 * width; x += 3)
		{
			i_triplet = y * 3 * width + x;
			i_bmp = (height - y - 1) * padded_width + x;
			triplet_arr[i_triplet] = bmp_data[i_bmp + 2];
			triplet_arr[i_triplet + 1] = bmp_data[i_bmp + 1];
			triplet_arr[i_triplet + 2] = bmp_data[i_bmp];
		}
	return triplet_arr;
}

// Adds padding and swaps R and B
BYTE* TripletsToBMP(BYTE* triplet_arr, int width, int height, long* newsize)
{
	int padding = 0;
	int scanlinebytes = width * 3; // size of one unpadded scanline in bytes
	while ((scanlinebytes + padding) % 4 != 0)
		padding++;
	int padded_width = scanlinebytes + padding;
	*newsize = height * padded_width;
	BYTE* bmp_data = malloc(*newsize);
	memset(bmp_data, 0, *newsize);

	long i_triplet = 0;
	long i_bmp = 0;
	for (int y = 0; y < height; y++)
		for (int x = 0; x < 3 * width; x += 3)
		{
			i_triplet = y * 3 * width + x;
			i_bmp = (height - y - 1) * padded_width + x;
			bmp_data[i_bmp] = triplet_arr[i_triplet + 2];
			bmp_data[i_bmp + 1] = triplet_arr[i_triplet + 1];
			bmp_data[i_bmp + 2] = triplet_arr[i_triplet];
		}
	return bmp_data;
}

char Save(BYTE* Buffer, int width, int height, long paddedsize, LPCTSTR bmpfile)
{
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER info;
	memset(&bmfh, 0, sizeof(BITMAPFILEHEADER));
	memset(&info, 0, sizeof(BITMAPINFOHEADER));
	bmfh.bfType = 0x4d42;       // 0x4d42 = 'BM'
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfSize = sizeof(BITMAPFILEHEADER) +
		sizeof(BITMAPINFOHEADER) + paddedsize;
	bmfh.bfOffBits = 0x36;
	info.biSize = sizeof(BITMAPINFOHEADER);
	info.biWidth = width;
	info.biHeight = height;
	info.biPlanes = 1;
	info.biBitCount = 24;
	info.biCompression = BI_RGB;
	info.biSizeImage = 0;
	info.biXPelsPerMeter = 0x0ec4;
	info.biYPelsPerMeter = 0x0ec4;
	info.biClrUsed = 0;
	info.biClrImportant = 0;
	HANDLE file = CreateFile(bmpfile, GENERIC_WRITE, FILE_SHARE_READ,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (NULL == file)
	{
		CloseHandle(file);
		return 0;
	}
	unsigned long bwritten;
	if (WriteFile(file, &bmfh, sizeof(BITMAPFILEHEADER),
		&bwritten, NULL) == 0)
	{
		CloseHandle(file);
		return 0;
	}

	if (WriteFile(file, &info, sizeof(BITMAPINFOHEADER),
		&bwritten, NULL) == 0)
	{
		CloseHandle(file);
		return 0;
	}
	if (WriteFile(file, Buffer, paddedsize, &bwritten, NULL) == 0)
	{
		CloseHandle(file);
		return 0;
	}
	CloseHandle(file);
	return 1;
}

RGBTriplet* scale_nearest(int wi, int hi, int wo, int ho, RGBTriplet * input) //nearest neighbor
{
	RGBTriplet* output = malloc(wo*ho * 3);
	double x = wi / (double)wo;
	double y = hi / (double)ho;
	double px, py;
	for (int i = 0; i < ho; i++)
	{
		for (int j = 0; j < wo; j++)
		{
			px = floor(j*x);
			py = floor(i*y);
			output[(i*wo) + j] = input[(int)((py*wi) + px)];
		}
	}
	return output;
}

RGBTriplet* simple_grayscale(int wi, int hi, RGBTriplet * input)
{
	RGBTriplet* output = malloc(wi*hi * 3);
	for (int i = 0; i < hi; i++)
	{
		for (int j = 0; j < wi; j++)
		{
			int index = (i*wi + j);
			byte val = input[index].red * 0.33 + input[index].blue * 0.33 + input[index].green * 0.33;
			output[index].blue = val;
			output[index].red = val;
			output[index].green = val;
		}
	}
	return output;
}


RGBTriplet* blur(int wi, int hi, RGBTriplet * input, int radius)
{
	RGBTriplet* output = malloc(wi*hi * 3);
	for (int xx = 0; xx < hi; xx++)
	{
		for (int yy = 0; yy < wi; yy++)
		{
			int avgR = 0;
			int avgG = 0;
			int avgB = 0;
			int c = 0;

			for (int x = xx; x < xx + radius && x < hi; x++)
			{
				for (int y = yy; y < yy + radius && y < wi; y++)
				{
					avgB += input[x * wi + y].blue;
					avgG += input[x * wi + y].green;
					avgR += input[x * wi + y].red;
					c++;
				}
			}

			avgB = avgB / c;
			avgG = avgG / c;
			avgR = avgR / c;

			output[xx * wi + yy].blue = avgB;
			output[xx * wi + yy].green = avgG;
			output[xx * wi + yy].red = avgR;
		}
	}
	return output;
}

RGBTriplet* hdtv_grayscale(int wi, int hi, RGBTriplet * input) {
	RGBTriplet* output = malloc(wi*hi * 3);
	int offset = 0;
	for (int i = 0; i < hi; i++)
		for (int j = 0; j < wi; j++) {
			int index = (i*wi + j);
			byte val = input[index].red * 0.2126 + input[index].blue * 0.0722
				+ input[index].green * 0.7152;
			output[index].blue = val;
			output[index].red = val;
			output[index].green = val;
		}
	return output;
}

RGBTriplet* scale_bilinear(int wi, int hi, int wo, int ho, RGBTriplet * input)
{
	RGBTriplet* output = malloc(wo*ho * 3);
	RGBTriplet a, b, c, d;
	int x, y, index;
	double x_ratio = ((double)(wi - 1)) / wo;
	double y_ratio = ((double)(hi - 1)) / ho;
	double x_diff, y_diff, blue, red, green;
	int offset = 0;
	for (int i = 0; i < ho; i++) {
		for (int j = 0; j < wo; j++) {
			x = (int)(x_ratio * j);
			y = (int)(y_ratio * i);
			x_diff = (x_ratio * j) - x;
			y_diff = (y_ratio * i) - y;
			index = (y*wi + x);
			a = input[index];
			b = input[index + 1];
			c = input[index + wi];
			d = input[index + wi + 1];

			blue = a.blue*(1 - x_diff)*(1 - y_diff) + b.blue*(x_diff)*(1 - y_diff) +
				c.blue*(y_diff)*(1 - x_diff) + d.blue*(x_diff*y_diff);

			green = a.green*(1 - x_diff)*(1 - y_diff) + b.green*(x_diff)*(1 - y_diff) +
				c.green*(y_diff)*(1 - x_diff) + d.green*(x_diff*y_diff);

			red = a.red*(1 - x_diff)*(1 - y_diff) + b.red*(x_diff)*(1 - y_diff) +
				c.red*(y_diff)*(1 - x_diff) + d.red*(x_diff*y_diff);

			output[offset].blue = blue;
			output[offset].green = green;
			output[offset].red = red;
			offset++;
		}
	}
	return output;
}


RGBTriplet* GetTripletArray(char* FileName, int * width, int * height)
{
	BITMAPFILEHEADER file_header;
	BITMAPINFOHEADER info_header;
	BITMAPINFO *pbmi;
	BYTE *pData;
	DWORD nRead;
	HANDLE hFile = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;
	ReadFile(hFile, &file_header, sizeof(BITMAPFILEHEADER), &nRead, NULL);
	if (file_header.bfType != 0x4D42)
		return NULL;
	ReadFile(hFile, &info_header, sizeof(BITMAPINFOHEADER), &nRead, NULL);
	if (info_header.biSize != 40)
		return NULL;

	pbmi = (BITMAPINFO*)malloc(sizeof(BITMAPINFO) + info_header.biClrUsed * sizeof(RGBQUAD));
	memcpy(pbmi, &info_header, sizeof(BITMAPINFOHEADER));

	ReadFile(hFile, pbmi->bmiColors, info_header.biClrUsed * sizeof(RGBQUAD), &nRead, NULL);
	pData = (BYTE*)malloc(info_header.biSizeImage);
	ReadFile(hFile, pData, info_header.biSizeImage, &nRead, NULL);
	CloseHandle(hFile);

	RGBTriplet* buffer = (RGBTriplet*)BMPToTriplets(pData, info_header.biWidth, info_header.biHeight);

	free(pbmi);
	free(pData);
	*height = info_header.biHeight;
	*width = info_header.biWidth;
	return buffer;
}


int main()
{
	char* name = "C:\\Misc\\input.bmp";
	int height;
	int width;
	RGBTriplet * original = GetTripletArray(name, &width, &height);

	double scalefact_x = 2;
	double scalefact_y = 2;

	RGBTriplet* scaled_nn = NULL;
	RGBTriplet* scaled_b = NULL;
	RGBTriplet* greyscale = NULL;
	RGBTriplet* greyscale_hdtv = NULL;
	RGBTriplet* blurred = NULL;

	scaled_nn = scale_nearest(width, height, width*scalefact_x, height*scalefact_y, original);
	scaled_b = scale_bilinear(width, height, width*scalefact_x, height*scalefact_y, original);
	greyscale = simple_grayscale(width, height, original);
	greyscale_hdtv = hdtv_grayscale(width, height, original);
	blurred = blur(width, height, original,9);

	long s1 = 0;
	long s2 = 0;
	long s3 = 0;
	long s4 = 0;
	long s5 = 0;


	BYTE* bmp_nn = TripletsToBMP(scaled_nn, width*scalefact_x, height*scalefact_y, &s1);
	Save(bmp_nn, width*scalefact_x, height*scalefact_y, s1, L"output_nn.bmp");

	BYTE* bmp_b = TripletsToBMP(scaled_b, width*scalefact_x, height*scalefact_y, &s2);
	Save(bmp_b, width*scalefact_x, height*scalefact_y, s2, L"output_b.bmp");

	BYTE* bmp_gs = TripletsToBMP(greyscale, width, height, &s3);
	Save(bmp_gs, width, height, s3, L"output_gs.bmp");

	BYTE* bmp_hdtv = TripletsToBMP(greyscale_hdtv, width, height, &s4);
	Save(bmp_hdtv, width, height, s4, L"output_hdtv.bmp");

	BYTE* bmp_blur = TripletsToBMP(blurred, width, height, &s5);
	Save(bmp_blur, width, height, s5, L"output_blur.bmp");
}


