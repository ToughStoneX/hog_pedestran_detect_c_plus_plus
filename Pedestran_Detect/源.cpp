#include "opencv2/opencv.hpp"
#include "opencv2/ml.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <cctype>
#include <windows.h>

using namespace std;

#define FILEPATH "E:/opencv/My OpenCV projects/HOG特征/HOG_Pedestran_Detect/Pedestran_Detect/Pedestran_Detect/Pedestrians64x128/"

void Train()
{
	////////////////////////////////读入训练样本图片路径和类别///////////////////////////////////////////////////
	//图像路径和类别
	vector<string> imagePath;
	vector<int> imageClass;
	string buffer;
	ifstream trainingData(string(FILEPATH) + "TrainData.txt");
	int numOfLine = 0;
	while (!trainingData.eof())
	{
		getline(trainingData, buffer);
		//cout << buffer << endl;
		if (!buffer.empty())
		{
			numOfLine++;
			if (numOfLine % 2 == 0)
			{
				//读取样本类别
				imageClass.push_back(atoi(buffer.c_str()));
			}
			else
			{
				//读取图像路径
				imagePath.push_back(buffer);
			}
		}
	}
	trainingData.close();

	////////////////////////////////获取样本的HOG特征///////////////////////////////////////////////////
	//样本特征向量矩阵
	int numOfSample = numOfLine / 2;
	cv::Mat featureVectorOfSample(numOfSample, 3780, CV_32FC1);

	//样本的类别
	cv::Mat classOfSample(numOfSample, 1, CV_32SC1);

	cv::Mat convertedImg;
	cv::Mat trainImg;

	for (vector<string>::size_type i = 0;i < imagePath.size();i++)
	{
		cout << "Processing: " << imagePath[i] << endl;
		cv::Mat src = cv::imread(imagePath[i], -1);
		if (src.empty())
		{
			cout << "can not load the image:" << imagePath[i] << endl;
			continue;
		}

		cv::resize(src, trainImg, cv::Size(64, 128));

		//提取HOG特征
		cv::HOGDescriptor hog(cv::Size(64, 128), cv::Size(16, 16), cv::Size(8, 8), cv::Size(8, 8), 9);
		vector<float> descriptors;
		hog.compute(trainImg, descriptors);

		cout << "hog feature vector: " << descriptors.size() << endl;

		for (vector<float>::size_type j = 0;j < descriptors.size();j++)
		{
			featureVectorOfSample.at<float>(i, j) = descriptors[j];
		}
		classOfSample.at<int>(i, 0) = imageClass[i];
	}

	cout << "size of featureVectorOfSample: " << featureVectorOfSample.size() << endl;
	cout << "size of classOfSample: " << classOfSample.size() << endl;

	///////////////////////////////////使用SVM分类器训练///////////////////////////////////////////////////
	//设置参数，注意Ptr的使用
	cv::Ptr<cv::ml::SVM> svm = cv::ml::SVM::create();
	svm->setType(cv::ml::SVM::C_SVC);
	svm->setKernel(cv::ml::SVM::LINEAR);
	svm->setTermCriteria(cv::TermCriteria(CV_TERMCRIT_ITER, 1000, FLT_EPSILON));

	//训练SVM
	svm->train(featureVectorOfSample, cv::ml::ROW_SAMPLE, classOfSample);

	//保存训练好的分类器（其中包含了SVM的参数，支持向量，α和rho）
	svm->save(string(FILEPATH) + "classifier.xml");

	/*
	SVM训练完成后得到的XML文件里面，有一个数组，叫做support vector，还有一个数组，叫做alpha,有一个浮点数，叫做rho;
	将alpha矩阵同support vector相乘，注意，alpha*supportVector,将得到一个行向量，将该向量前面乘以-1。之后，再该行向量的最后添加一个元素rho。
	如此，变得到了一个分类器，利用该分类器，直接替换opencv中行人检测默认的那个分类器（cv::HOGDescriptor::setSVMDetector()），
	*/
	//获取支持向量
	cv::Mat supportVector = svm->getSupportVectors();

	//获取alpha和rho
	cv::Mat alpha;
	cv::Mat svIndex;
	float rho = svm->getDecisionFunction(0, alpha, svIndex);

	//转换类型:这里一定要注意，需要转换为32的
	cv::Mat alpha2;
	alpha.convertTo(alpha2, CV_32FC1);

	//结果矩阵，两个矩阵相乘
	cv::Mat result(1, 3780, CV_32FC1);
	result = alpha2 * supportVector;

	//乘以-1，这里为什么会乘以-1？
	//注意因为svm.predict使用的是alpha*sv*another-rho，如果为负的话则认为是正样本，在HOG的检测函数中，使用rho+alpha*sv*another(another为-1)
	//for (int i = 0;i < 3780;i++)
		//result.at<float>(0, i) *= -1;

	//将分类器保存到文件，便于HOG识别
	//这个才是真正的判别函数的参数(ω)，HOG可以直接使用该参数进行识别
	FILE *fp = fopen((string(FILEPATH) + "HOG_SVM.txt").c_str(), "wb");
	for (int i = 0; i<3780; i++)
	{
		fprintf(fp, "%f \n", result.at<float>(0, i));
	}
	fprintf(fp, "%f", rho);

	fclose(fp);
}

void Detect()
{
	cv::Mat img;
	FILE* f = 0;
	char _filename[1024];

	// 获取测试图片文件路径
	f = fopen((string(FILEPATH) + "TestData.txt").c_str(), "rt");
	if (!f)
	{
		fprintf(stderr, "ERROR: the specified file could not be loaded\n");
		return;
	}

	//加载训练好的判别函数的参数(注意，与svm->save保存的分类器不同)
	vector<float> detector;
	ifstream fileIn(string(FILEPATH) + "HOG_SVM.txt", ios::in);
	float val = 0.0f;
	while (!fileIn.eof())
	{
		fileIn >> val;
		detector.push_back(val);
	}
	fileIn.close();

	//设置HOG
	cv::HOGDescriptor hog;
	//hog.setSVMDetector(detector);
	hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
	cv::namedWindow("people detector", 1);

	// 检测图片
	for (;;)
	{
		// 读取文件名
		char* filename = _filename;
		if (f)
		{
			if (!fgets(filename, (int)sizeof(_filename) - 2, f))
				break;
			if (filename[0] == '#')
				continue;

			//去掉空格
			int l = (int)strlen(filename);
			while (l > 0 && isspace(filename[l - 1]))
				--l;
			filename[l] = '\0';
			img = cv::imread(filename);
		}
		printf("%s:\n", filename);
		if (!img.data)
			continue;

		fflush(stdout);
		vector<cv::Rect> found, found_filtered;
		// run the detector with default parameters. to get a higher hit-rate
		// (and more false alarms, respectively), decrease the hitThreshold and
		// groupThreshold (set groupThreshold to 0 to turn off the grouping completely).
		//多尺度检测
		hog.detectMultiScale(img, found, 0, cv::Size(8, 8), cv::Size(32, 32), 1.05, 2);

		size_t i, j;
		//去掉空间中具有内外包含关系的区域，保留大的
		for (i = 0; i < found.size(); i++)
		{
			cv::Rect r = found[i];
			for (j = 0; j < found.size(); j++)
				if (j != i && (r & found[j]) == r)
					break;
			if (j == found.size())
				found_filtered.push_back(r);
		}

		// 适当缩小矩形
		for (i = 0; i < found_filtered.size(); i++)
		{
			cv::Rect r = found_filtered[i];
			// the HOG detector returns slightly larger rectangles than the real objects.
			// so we slightly shrink the rectangles to get a nicer output.
			r.x += cvRound(r.width*0.1);
			r.width = cvRound(r.width*0.8);
			r.y += cvRound(r.height*0.07);
			r.height = cvRound(r.height*0.8);
			rectangle(img, r.tl(), r.br(), cv::Scalar(0, 255, 0), 3);
		}

		imshow("people detector", img);
		int c = cv::waitKey(0) & 255;
		if (c == 'q' || c == 'Q' || !f)
			break;
	}
	if (f)
		fclose(f);
	return;
}

int main()
{
	Train();

	Detect();

	return 0;
}