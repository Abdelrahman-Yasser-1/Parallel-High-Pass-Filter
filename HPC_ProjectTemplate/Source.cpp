#include <iostream>
#include <math.h>
#include <stdlib.h>
#include<string.h>
#include<msclr\marshal_cppstd.h>
#include<mpi.h>
#include <ctime>// include this header 
#pragma once

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace std;
using namespace msclr::interop;

int* inputImage(int* w, int* h, System::String^ imagePath) //put the size of image in w & h
{
	int* input;


	int OriginalImageWidth, OriginalImageHeight;

	//*********************************************************Read Image and save it to local arrayss*************************	
	//Read Image and save it to local arrayss

	System::Drawing::Bitmap BM(imagePath);

	OriginalImageWidth = BM.Width;
	OriginalImageHeight = BM.Height;
	*w = BM.Width;
	*h = BM.Height;
	int* Red = new int[BM.Height * BM.Width];
	int* Green = new int[BM.Height * BM.Width];
	int* Blue = new int[BM.Height * BM.Width];
	input = new int[BM.Height * BM.Width];
	for (int i = 0; i < BM.Height; i++)
	{
		for (int j = 0; j < BM.Width; j++)
		{
			System::Drawing::Color c = BM.GetPixel(j, i);

			Red[i * BM.Width + j] = c.R;
			Blue[i * BM.Width + j] = c.B;
			Green[i * BM.Width + j] = c.G;

			input[i * BM.Width + j] = ((c.R + c.B + c.G) / 3); //gray scale value equals the average of RGB values

		}

	}
	return input;
}

void createImage(int* image, int width, int height, int index)
{
	System::Drawing::Bitmap MyNewImage(width, height);


	for (int i = 0; i < MyNewImage.Height; i++)
	{
		for (int j = 0; j < MyNewImage.Width; j++)
		{
			//i * OriginalImageWidth + j
			if (image[i * width + j] < 0)
			{
				image[i * width + j] = 0;
			}
			if (image[i * width + j] > 255)
			{
				image[i * width + j] = 255;
			}
			System::Drawing::Color c = System::Drawing::Color::FromArgb(image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j]);
			MyNewImage.SetPixel(j, i, c);
		}
	}
	MyNewImage.Save("..//Data//Output//outputRes" + index + ".png");
	cout << "result Image Saved " << index << endl;
}

int* apply_padding(int* image, int width, int height)
{
	int padded_width = width + 2, padded_height = height + 2;
	long long pad_size = padded_width * padded_height;
	int* padded_image = new int[pad_size]();
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			padded_image[(i + 1) * padded_width + j + 1] = image[i * width + j];
		}
	}
	return padded_image;
}

int main()
{
	MPI_Init(NULL, NULL);
	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	int ImageWidth = 4, ImageHeight = 4;
	int imageSize;

	int start_s, stop_s, TotalTime = 0;
	int* imageData = 0;

	const int Tag_Start_idx = 1;
	const int Tag_Processor_Rows = 2;
	const int Tag_Sub_Image = 3;
	const int Tag_image_width = 4;
	const int Tag_image_height = 5;
	const int Tag_local_size = 6;
	const int Tag_local_result = 7;
	const int Tag_Last_Processor = 8;
	const int Tag_Last_Processor_Sub_Size = 9;
	const int Tag_Sub_Image_Size = 10;


	int kernal[9] = { 0,-20,0,-20,80,-20,0,-20,0 };
	MPI_Bcast(&kernal, 9, MPI_INT, 0, MPI_COMM_WORLD);

	int* paddedImage;
	int start_idx, sub_image_size, padded_height, padded_width;
	int* Sub_Image;
	int* localResult;
	int start_index = 0, rows = 0;

	MPI_Status status;
	std::string name = "test2";
	//send the data to the rest of the processes
	if (rank == 0)
	{
		System::String^ imagePath;
		std::string img;
		img = "..//Data//Input//" + name + ".png";

		imagePath = marshal_as<System::String^>(img);
		int* imageData = inputImage(&ImageWidth, &ImageHeight, imagePath);
		imageSize = ImageWidth * ImageHeight;
		start_s = clock();

		padded_width = ImageWidth + 2;
		padded_height = ImageHeight + 2;
		int padded_image_size = padded_width * padded_height;

		paddedImage = apply_padding(imageData, ImageWidth, ImageHeight);

		for (int i = 1; i < size; i++)
		{
			rows = padded_height / (size - 1);

			start_index = (i - 1) * (rows * padded_width);

			if (i != 1)
				start_index -= padded_width;

			if (i == 1 || i == (size - 1)) {
				rows++;
				sub_image_size = padded_width * (rows);
			}
			else {
				rows += 2;
				sub_image_size = padded_width * (rows);
			}

			MPI_Send(&ImageHeight, 1, MPI_INT, i, Tag_image_height, MPI_COMM_WORLD);
			MPI_Send(&ImageWidth, 1, MPI_INT, i, Tag_image_width, MPI_COMM_WORLD);
			MPI_Send(&rows, 1, MPI_INT, i, Tag_Processor_Rows, MPI_COMM_WORLD);
			MPI_Send(&sub_image_size, 1, MPI_INT, i, Tag_Sub_Image_Size, MPI_COMM_WORLD);
			MPI_Send(&paddedImage[start_index], sub_image_size, MPI_INT, i, Tag_Sub_Image, MPI_COMM_WORLD);

		}
	}
	else if (rank == 1 || rank == size - 1) {

		int imageWidth = 0, imageHeight = 0;
		MPI_Recv(&imageHeight, 1, MPI_INT, 0, Tag_image_height, MPI_COMM_WORLD, &status);
		MPI_Recv(&imageWidth, 1, MPI_INT, 0, Tag_image_width, MPI_COMM_WORLD, &status);
		MPI_Recv(&rows, 1, MPI_INT, 0, Tag_Processor_Rows, MPI_COMM_WORLD, &status);
		MPI_Recv(&sub_image_size, 1, MPI_INT, 0, Tag_Sub_Image_Size, MPI_COMM_WORLD, &status);
		Sub_Image = new int[sub_image_size];
		MPI_Recv(Sub_Image, sub_image_size, MPI_INT, 0, Tag_Sub_Image, MPI_COMM_WORLD, &status);

		int paddedwidth = imageWidth + 2, paddedheight = imageHeight + 2;

		int localSize = sub_image_size - (2 * imageWidth) - (2 * rows);

		int* localResult1 = new int[localSize];
		int counter = 0;
		int index = 0;
		for (int i = 0; i < (rows * paddedwidth) - (2 * paddedwidth); i++)
		{
			if (index == imageWidth)
			{
				i++;
				index = 0;
				continue;
			}
			int result = (Sub_Image[i] * kernal[0]) + (Sub_Image[i + 1] * kernal[1]) + (Sub_Image[i + 2] * kernal[2]) +
				         (Sub_Image[i + paddedwidth] * kernal[3]) + (Sub_Image[i + paddedwidth + 1] * kernal[4]) + (Sub_Image[i + paddedwidth + 2] * kernal[5]) +
				         (Sub_Image[i + (2 * paddedwidth)] * kernal[6]) + (Sub_Image[i + (2 * paddedwidth) + 1] * kernal[7]) + (Sub_Image[i + (2 * paddedwidth) + 2] * kernal[8]);

			localResult1[counter] = result;
			counter++;
			index++;
		}

		MPI_Send(&localSize, 1, MPI_INT, 0, Tag_local_size, MPI_COMM_WORLD);
		MPI_Send(localResult1, localSize, MPI_INT, 0, 20, MPI_COMM_WORLD);
		//createImage(localResult1, paddedwidth - 2, rows - 2, rank);
	}
	else {
		int imageWidth = 0, imageHeight = 0;

		MPI_Recv(&imageHeight, 1, MPI_INT, 0, Tag_image_height, MPI_COMM_WORLD, &status);
		MPI_Recv(&imageWidth, 1, MPI_INT, 0, Tag_image_width, MPI_COMM_WORLD, &status);
		MPI_Recv(&rows, 1, MPI_INT, 0, Tag_Processor_Rows, MPI_COMM_WORLD, &status);
		MPI_Recv(&sub_image_size, 1, MPI_INT, 0, Tag_Sub_Image_Size, MPI_COMM_WORLD, &status);
		int* Sub_Image1 = new int[sub_image_size];
		MPI_Recv(Sub_Image1, sub_image_size, MPI_INT, 0, Tag_Sub_Image, MPI_COMM_WORLD, &status);

		int paddedwidth = imageWidth + 2, paddedheight = imageHeight + 2;

		int localSize = sub_image_size - (2 * imageWidth) - (2 * rows);

		int* localResult2 = new int[localSize];
		int counter = 0;
		int index = 0;

		for (int i = 0; i < (rows * paddedwidth) - (2 * paddedwidth); i++)
		{
			if (index == imageWidth)
			{
				i++;
				index = 0;
				continue;
			}

			int result = (Sub_Image1[i] * kernal[0]) + (Sub_Image1[i + 1] * kernal[1]) + (Sub_Image1[i + 2] * kernal[2]) +
				         (Sub_Image1[i + paddedwidth] * kernal[3]) + (Sub_Image1[i + paddedwidth + 1] * kernal[4]) + (Sub_Image1[i + paddedwidth + 2] * kernal[5]) +
				         (Sub_Image1[i + (2 * paddedwidth)] * kernal[6]) + (Sub_Image1[i + (2 * paddedwidth) + 1] * kernal[7]) + (Sub_Image1[i + (2 * paddedwidth) + 2] * kernal[8]);

			localResult2[counter] = result;
			counter++;
			index++;
		}
		//createImage(localResult2, paddedwidth - 2, rows - 2, rank);
		MPI_Send(&localSize, 1, MPI_INT, 0, Tag_local_size, MPI_COMM_WORLD);
		MPI_Send(localResult2, localSize, MPI_INT, 0, 20, MPI_COMM_WORLD);
	}

	if (rank == 0) {
		int* finalResult = new int[10000*10000];
		int* local;
		int lsize;
		int counter = 0;
		for (int i = 1; i < size; i++)
		{
			lsize = 0;
			MPI_Recv(&lsize, 1, MPI_INT, i, Tag_local_size, MPI_COMM_WORLD, &status);
			local = new int[lsize];

			MPI_Recv(local, lsize, MPI_INT, i, 20, MPI_COMM_WORLD, &status);

			for (int j = 0; j < lsize; j++)
			{
				finalResult[counter] = local[j];
				counter++;
			}
		}
		createImage(finalResult, ImageWidth, ImageHeight, 101);
		stop_s = clock();
		TotalTime += (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
		cout << name;
		if (size == 2)
			cout << "  Sequential ";
		else
			cout << "  Parallel ";
		cout << "time: " << TotalTime << endl;
	}
	MPI_Finalize();
	/*free(imageData);
	free(paddedImage);
	free(localResult);
	free(finalResult);
	free(Sub_Image);*/
	return 0;
}