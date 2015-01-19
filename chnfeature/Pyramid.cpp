#include <iostream>
#include <fstream> 
#include <cmath>
#include <vector>
#include <typeinfo>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/opencv.hpp" 
#include <cv.h>
#include <cxcore.h> 
#include <cvaux.h>
#include "Pyramid.h"


using namespace std;
using namespace cv;
void feature_Pyramids::convTri( const Mat &src, Mat &dst,Mat const &Km) const
{
	
	filter2D(src,dst,src.depth(),Km,Point(-1,-1),0,IPL_BORDER_REFLECT);
	filter2D(dst,dst,src.depth(),Km.t(),Point(-1,-1),0,IPL_BORDER_REFLECT); 
	
}
void feature_Pyramids::computeGradient(const Mat &img, Mat& grad, Mat& qangle) const
{
	bool gammaCorrection = false;
    Size paddingTL=Size(0,0);
	Size paddingBR=Size(0,0);
	int nbins=m_opt.nbins;
	//CV_Assert( img.type() == CV_8U || img.type() == CV_8UC3 );
	CV_Assert( img.type() == CV_32F || img.type() == CV_32FC3 );

	Size gradsize(img.cols + paddingTL.width + paddingBR.width,
		img.rows + paddingTL.height + paddingBR.height);
	grad.create(gradsize, CV_32FC2);  // <magnitude*(1-alpha), magnitude*alpha>
	qangle.create(gradsize, CV_8UC2); // [0..nbins-1] - quantized gradient orientation
	Size wholeSize;
	Point roiofs;
	img.locateROI(wholeSize, roiofs);

	int x, y;
	int cn = img.channels();

	AutoBuffer<int> mapbuf(gradsize.width + gradsize.height + 4);
	int* xmap = (int*)mapbuf + 1;
	int* ymap = xmap + gradsize.width + 2;

	const int borderType = (int)BORDER_REFLECT_101;
	//! 1D interpolation function: returns coordinate of the "donor" pixel for the specified location p.
	for( x = -1; x < gradsize.width + 1; x++ )
		xmap[x] = borderInterpolate(x - paddingTL.width + roiofs.x,
		wholeSize.width, borderType) - roiofs.x;
	for( y = -1; y < gradsize.height + 1; y++ )
		ymap[y] = borderInterpolate(y - paddingTL.height + roiofs.y,
		wholeSize.height, borderType) - roiofs.y;

	// x- & y- derivatives for the whole row
	int width = gradsize.width;
	AutoBuffer<float> _dbuf(width*4);
	float* dbuf = _dbuf;
	Mat Dx(1, width, CV_32F, dbuf);
	Mat Dy(1, width, CV_32F, dbuf + width);
	Mat Mag(1, width, CV_32F, dbuf + width*2);
	Mat Angle(1, width, CV_32F, dbuf + width*3);

	int _nbins = nbins;
	float angleScale = (float)(_nbins/(2*CV_PI));
#ifdef HAVE_IPP
	Mat lutimg(img.rows,img.cols,CV_MAKETYPE(CV_32F,cn));
	Mat hidxs(1, width, CV_32F);
	Ipp32f* pHidxs  = (Ipp32f*)hidxs.data;
	Ipp32f* pAngles = (Ipp32f*)Angle.data;

	IppiSize roiSize;
	roiSize.width = img.cols;
	roiSize.height = img.rows;

	for( y = 0; y < roiSize.height; y++ )
	{
		const uchar* imgPtr = img.data + y*img.step;
		float* imglutPtr = (float*)(lutimg.data + y*lutimg.step);

		for( x = 0; x < roiSize.width*cn; x++ )
		{
			imglutPtr[x] = lut[imgPtr[x]];
		}
	}

#endif
	//vector<Mat> angle;
	//Mat combineMat;
	for( y = 0; y < gradsize.height; y++ )
	{
#ifdef HAVE_IPP
		const float* imgPtr  = (float*)(lutimg.data + lutimg.step*ymap[y]);
		const float* prevPtr = (float*)(lutimg.data + lutimg.step*ymap[y-1]);
		const float* nextPtr = (float*)(lutimg.data + lutimg.step*ymap[y+1]);
#else
		const float* imgPtr  = (float*)(img.data + img.step*ymap[y]);
		const float* prevPtr = (float*)(img.data + img.step*ymap[y-1]);
		const float* nextPtr = (float*)(img.data + img.step*ymap[y+1]);
#endif
		float* gradPtr = (float*)grad.ptr(y);
		uchar* qanglePtr = (uchar*)qangle.ptr(y);

		if( cn == 1 )
		{
			for( x = 0; x < width; x++ )
			{
				int x1 = xmap[x];
#ifdef HAVE_IPP
				dbuf[x] = (float)(imgPtr[xmap[x+1]] - imgPtr[xmap[x-1]]);
				dbuf[width + x] = (float)(nextPtr[x1] - prevPtr[x1]);
#else
				dbuf[x] = (float)(imgPtr[xmap[x+1]] - imgPtr[xmap[x-1]])*0.5f;
				dbuf[width + x] = (float)(nextPtr[x1] - prevPtr[x1])*0.5f; //??
#endif
			}
		}
		else
		{
			for( x = 0; x < width; x++ )
			{
				int x1 = xmap[x]*3;
				float dx0, dy0, dx, dy, mag0, mag;
#ifdef HAVE_IPP
				const float* p2 = imgPtr + xmap[x+1]*3;
				const float* p0 = imgPtr + xmap[x-1]*3;

				dx0 = p2[2] - p0[2];
				dy0 = nextPtr[x1+2] - prevPtr[x1+2];
				mag0 = dx0*dx0 + dy0*dy0;

				dx = p2[1] - p0[1];
				dy = nextPtr[x1+1] - prevPtr[x1+1];
				mag = dx*dx + dy*dy;

				if( mag0 < mag )
				{
					dx0 = dx;
					dy0 = dy;
					mag0 = mag;
				}

				dx = p2[0] - p0[0];
				dy = nextPtr[x1] - prevPtr[x1];
				mag = dx*dx + dy*dy;
#else
				const float* p2 = imgPtr + xmap[x+1]*3;
				const float* p0 = imgPtr + xmap[x-1]*3;

				dx0 = (p2[2] - p0[2])*0.5f;
				dy0 = (nextPtr[x1+2] - prevPtr[x1+2])*0.5f;
				mag0 = dx0*dx0 + dy0*dy0;

				dx = (p2[1] - p0[1])*0.5f;
				dy = (nextPtr[x1+1] - prevPtr[x1+1])*0.5f;
				mag = dx*dx + dy*dy;

				if( mag0 < mag )
				{
					dx0 = dx;
					dy0 = dy;
					mag0 = mag;
				}

				dx = (p2[0] - p0[0])*0.5f;
				dy = (nextPtr[x1] - prevPtr[x1])*0.5f;
				mag = dx*dx + dy*dy;
#endif
				if( mag0 < mag )
				{
					dx0 = dx;
					dy0 = dy;
					mag0 = mag;
				}

				dbuf[x] = dx0;
				dbuf[x+width] = dy0;
			}
		}
#ifdef HAVE_IPP
		ippsCartToPolar_32f((const Ipp32f*)Dx.data, (const Ipp32f*)Dy.data, (Ipp32f*)Mag.data, pAngles, width);
		for( x = 0; x < width; x++ )
		{
			if(pAngles[x] < 0.f)
				pAngles[x] += (Ipp32f)(CV_PI*2.);
		}

		ippsNormalize_32f(pAngles, pAngles, width, 0.5f/angleScale, 1.f/angleScale);
		ippsFloor_32f(pAngles,(Ipp32f*)hidxs.data,width);
		ippsSub_32f_I((Ipp32f*)hidxs.data,pAngles,width);
		ippsMul_32f_I((Ipp32f*)Mag.data,pAngles,width);

		ippsSub_32f_I(pAngles,(Ipp32f*)Mag.data,width);
		ippsRealToCplx_32f((Ipp32f*)Mag.data,pAngles,(Ipp32fc*)gradPtr,width);
#else
		cartToPolar( Dx, Dy, Mag, Angle, false );
#endif


		for( x = 0; x < width; x++ )
		{
#ifdef HAVE_IPP
			int hidx = (int)pHidxs[x];
#else
			float mag = dbuf[x+width*2], angle = dbuf[x+width*3]*angleScale - 0.5f;
			int hidx = cvFloor(angle);
			angle -= hidx;
			gradPtr[x*2] = mag*(1.f - angle);
			gradPtr[x*2+1] = mag*angle;
#endif
			if( hidx < 0 )
				hidx += _nbins;
			else if( hidx >= _nbins )
				hidx -= _nbins;
			assert( (unsigned)hidx < (unsigned)_nbins );

			qanglePtr[x*2] = (uchar)hidx;
			hidx++;
			hidx &= hidx < _nbins ? -1 : 0;
			qanglePtr[x*2+1] = (uchar)hidx;
		}
	}

}
void feature_Pyramids::computeChannels(const Mat &image,vector<Mat>& channels) const
{
    Mat Km;
	Mat img_tmp;
	double *kern=new double[2*m_opt.smooth+1];
	for (int c=0;c<=m_opt.smooth;c++)
	{
		kern[c]=(double)((c+1)/((m_opt.smooth+1.0)*(m_opt.smooth+1.0)));
		kern[2*m_opt.smooth-c]=kern[c];
	}
	Km=Mat(1,(2*m_opt.smooth+1),CV_64FC1,kern); 

	/*set para*/
	int nbins=m_opt.nbins;
	int binsize=m_opt.binsize;
	int shrink =m_opt.shrink;
	/* compute luv and push */
	Mat_<double> grad;
	Mat_<double> angles;
	Mat gray,src,luv;

	//int channels_addr_rows=(image.rows+binsize-1)/binsize;
	//int channels_addr_cols=(image.cols+binsize-1)/binsize;//*///15.0113
	int channels_addr_rows=(image.rows+shrink-1)/shrink;
	int channels_addr_cols=(image.cols+shrink-1)/shrink;
	Mat channels_addr=Mat::zeros((nbins+4)*channels_addr_rows,channels_addr_cols,CV_32FC1);
	if(image.channels() > 1)
	{
		src = Mat(image.rows, image.cols, CV_32FC3);
		image.convertTo(src, CV_32FC3, 1./255);
		cv::cvtColor(src, gray, CV_RGB2GRAY);
		cv::cvtColor(src, luv, CV_RGB2Luv);
	}else{
		src = Mat(image.rows, image.cols, CV_32FC1);
		image.convertTo(src, CV_32FC1, 1./255);
		src.copyTo(gray);
	}
	channels.clear();
	Mat luv_src;
	if(image.channels() > 1)
	{
		Mat luv_channels[3];
		cv::split(luv, luv_channels);
	
		/*  0<L<100, -134<u<220 -140<v<122  */
		/*  normalize to [0, 1] */
		luv_channels[0] *= 1.0/354;
		luv_channels[1] = (luv_channels[1]+134)/(354.0);
		luv_channels[2] = (luv_channels[2]+140)/(354.0);
        convTri( luv_channels[0], luv_channels[0], Km);
        convTri( luv_channels[1], luv_channels[1], Km);
        convTri( luv_channels[2], luv_channels[2], Km);
        
        vector<Mat> ttt;
        ttt.push_back( luv_channels[0]);
        ttt.push_back( luv_channels[1]);
        ttt.push_back( luv_channels[2]);

        cv::merge(ttt, luv_src);

		for( int i = 0; i < 3; ++i )
		{
			Mat channels_tmp=channels_addr.rowRange(i*channels_addr_rows,(i+1)*channels_addr_rows);
		    resize(luv_channels[i],channels_tmp,channels_tmp.size(),0.0,0.0,1);
			channels.push_back(channels_tmp);
		}
	}

	/*compute gradient*/
	Mat mag,ori;
	Mat mag_tmp;
	Mat mag_sum=channels_addr.rowRange(3*channels_addr_rows,4*channels_addr_rows);
	
    luv_src.convertTo(luv_src,CV_32FC1);

	computeGradient(luv_src, mag, ori);//1yue16//mzx
    
    /*  revised by YuanYang, test */
   

	//test
	/*Mat mag_sum_tmp;
	vector<Mat> mag_split_tmp;
	cv::split(mag,mag_split_tmp);
	mag_sum_tmp=mag_split_tmp[0]+mag_split_tmp[1];
	FileStorage fs2( " bin.yml" ,FileStorage::WRITE);
	fs2 << "data" << mag_sum_tmp;*/
	//test
	resize(mag,mag_tmp,mag_sum.size(),0.0,0.0,1);
	vector<Mat> mag_split;
	cv::split(mag_tmp, mag_split);//test
	mag_sum=mag_split[0]+mag_split[1];
	
    int t_smooth =5;
    double *t_kern=new double[2*t_smooth+1];
	for (int c=0;c<=t_smooth;c++)
	{
		t_kern[c]=(double)((c+1)/((t_smooth+1.0)*(t_smooth+1.0)));
		t_kern[2*t_smooth-c]=t_kern[c];
	}
	Mat t_Km=Mat(1,(2*t_smooth+1),CV_64FC1,t_kern); 
    Mat dst;
    convTri( mag_sum, dst, t_Km);
    mag_sum = mag_sum/( dst + 0.005);
    delete [] t_kern;
    
	channels.push_back(mag_sum);

	/*compute grad_hist*/
	vector<Mat> bins_mat,bins_mat_tmp;
	int bins_mat_tmp_rows=(mag.rows+binsize-1)/binsize;
	int bins_mat_tmp_cols=(mag.cols+binsize-1)/binsize;
	for( int s=0;s<nbins;s++){
		Mat channels_tmp=channels_addr.rowRange((s+4)*channels_addr_rows,(s+5)*channels_addr_rows);
		if (binsize==shrink)
		{
			bins_mat_tmp.push_back(channels_tmp);
		}else{
			bins_mat.push_back(channels_tmp);
			bins_mat_tmp.push_back(Mat::zeros(bins_mat_tmp_rows,bins_mat_tmp_rows,CV_32FC1));
		}
	}
	/*split*/
#define GH \
	bins_mat_tmp[ori.at<Vec2b>(row,col)[0]].at<float>((row)/binsize,(col)/binsize)+=mag.at<Vec2f>(row,col)[0];\
	bins_mat_tmp[ori.at<Vec2b>(row,col)[1]].at<float>((row)/binsize,(col)/binsize)+=mag.at<Vec2f>(row,col)[1];
	for(int row=0;row<mag.rows;row++){
		for(int col=0;col<mag.cols;col++){GH;}}
	/*push*/
	for (int c=0;c < (int)nbins;c++)
	{
		/*resize*/
		if (binsize==shrink)
		{
			channels.push_back(bins_mat_tmp[c]);
		}else{
			resize(bins_mat_tmp[c],bins_mat[c],bins_mat[c].size(),0.0,0.0,1);
			channels.push_back(bins_mat[c]);
		}
		
	}
	/*  check the pointer */
	/*float *add1 = (float*)channels[0].data;
	for( int c=1;c<channels.size();c++)
	{
	cout<<"pointer "<<(static_cast<void*>(channels[c].data) == (static_cast<void*>(add1+c*channels_addr_rows*channels_addr_cols))? true :false)<<endl;
	}*/
}
void feature_Pyramids:: chnsPyramid(const Mat &img,vector<vector<Mat> > &approxPyramid,vector<double> &scales,vector<double> &scalesh,vector<double> &scalesw) const
{
	int nPerOct =m_opt.nPerOct;
	int nOctUp =m_opt.nOctUp;
	int shrink =m_opt.shrink;
	int smooth =m_opt.smooth;
	int nbins=m_opt.nbins;
	int binsize=m_opt.binsize;
	int nApprox=m_opt.nApprox;
	Size minDS =m_opt.minDS;
	/*get scales*/
	//double minDs=(double)min(diam.width,diam.height);
	int nscales=(int)floor(nPerOct*(nOctUp+log(min(img.cols/(minDS.height*1.0),img.rows/(minDS.width*1.0)))/log(2))+1);
	vector<Size> ap_size;
	Size ap_tmp_size;

	approxPyramid.clear();
	scales.clear();
	scalesh.clear();
	scalesw.clear();

	CV_Assert(nApprox<nscales);
	/*compute real & approx scales*/
	vector<int> real_scal;
	double d0=(double)min(img.rows,img.cols);
	double d1=(double)max(img.rows,img.cols);
	for (double s=0;s<nscales;s++)
	{
		
		/*adjust ap_size*/
		double sc=pow(2.0,(-s)/nPerOct+nOctUp);
		double s0=(cvRound(d0*sc/shrink)*shrink-0.25*shrink)/d0;
		double s1=(cvRound(d0*sc/shrink)*shrink+0.25*shrink)/d0;
		double ss,es1,es2,a=10,val;
		for(int c=0;c<101;c++)
		{
			ss=(double)((s1-s0)*c/101+s0);
			es1=abs(d0*ss-cvRound(d0*ss/shrink)*shrink);
			es2=abs(d1*ss-cvRound(d1*ss/shrink)*shrink);
			if (max(es1,es2)<a)
			{
				a=max(es1,es2);
				val=ss;
			}
		}
		if (scales.empty())
		{
			/*all scales*/
			scales.push_back(val);
			scalesh.push_back(cvRound(img.rows*val/shrink)*shrink/(img.rows*1.0));
			scalesw.push_back(cvRound(img.cols*val/shrink)*shrink/(img.cols*1.0));
			/*save ap_size*/
			ap_tmp_size.height=cvRound(((img.rows+shrink-1)/shrink)*val);
			ap_tmp_size.width=cvRound(((img.cols+shrink-1)/shrink)*val);
			ap_size.push_back(ap_tmp_size);

		}else{
			if (val!=scales.back())
			{	
				/*all scales*/
				scales.push_back(val);
				scalesh.push_back(cvRound(img.rows*val/shrink)*shrink/(img.rows*1.0));
				scalesw.push_back(cvRound(img.cols*val/shrink)*shrink/(img.cols*1.0));
				/*save ap_size*/
				ap_tmp_size.height=cvRound(((img.rows+shrink-1)/shrink)*val);
				ap_tmp_size.width=cvRound(((img.cols+shrink-1)/shrink)*val);
				ap_size.push_back(ap_tmp_size);
			}
		}	
	}
	nscales=scales.size();
	for (int s=0;s<scales.size();s++)
	{
		/*real scale*/
		if (((int)s%(nApprox+1)==0))
		{
			real_scal.push_back((int)s);
		}
		if ((s==(scales.size()-1)&&(s>real_scal[real_scal.size()-1])&&(s-real_scal[real_scal.size()-1]>(nApprox+1)/2)))
		{
			real_scal.push_back((int)s);
		}
	}
	//compute the filter
	Mat Km;
	Mat img_tmp;
	double *kern=new double[2*smooth+1];
	for (int c=0;c<=smooth;c++)
	{
		kern[c]=(double)((c+1)/((smooth+1.0)*(smooth+1.0)));
		kern[2*smooth-c]=kern[c];
	}
	Km=Mat(1,(2*smooth+1),CV_64FC1,kern); 
	//convTri(img,img_tmp,Km);
    img_tmp  = img;
	//compute real 
	vector<vector<Mat> > chns_Pyramid;
	int chns_num;
	for (int s_r=0;s_r<(int)real_scal.size();s_r++)
	{
		vector<Mat> chns;
		resize(img_tmp,img_tmp,ap_size[real_scal[s_r]]*shrink,0.0,0.0,1);
		computeChannels(img_tmp,chns);
		chns_num=chns.size();
		chns_Pyramid.push_back(chns);
	}
	//compute lambdas
	vector<double> lambdas;
	if (lam.empty()) 
	{
		Scalar lam_s;
		Scalar lam_ss;
		CV_Assert(chns_Pyramid.size()>=2);
		if (chns_Pyramid.size()>2)
		{
			//compute lambdas
			     double size1,size2,lam_tmp;
				 size1 =(double)chns_Pyramid[1][0].rows*chns_Pyramid[1][0].cols;
				 size2 =(double)chns_Pyramid[2][0].rows*chns_Pyramid[2][0].cols;
				    //compute luv	 
					for (int c=0;c<3;c++)
					{
						lam_s+=sum(chns_Pyramid[1][c]);		
						lam_ss+=sum(chns_Pyramid[2][c]);
					}
					lam_s=lam_s/(size1*3.0);
					lam_ss=lam_ss/(size2*3.0);
					lam_tmp=-cv::log(lam_ss.val[0]/lam_s.val[0])/cv::log(scales[real_scal[2]]/scales[real_scal[1]]);
					for (int c=0;c<3;c++)
					{
						lambdas.push_back(lam_tmp);
					}
			        //compute  mag
					lam_s=sum(chns_Pyramid[1][4])/(size1*1.0);
					lam_ss=sum(chns_Pyramid[2][4])/(size2*1.0);
					lambdas.push_back(-cv::log(lam_ss.val[0]/lam_s.val[0])/cv::log(scales[real_scal[2]]/scales[real_scal[1]]));
			         //compute grad_hist
					for (int c=4;c<10;c++)
					{
						lam_s+=sum(chns_Pyramid[1][c]);		
						lam_ss+=sum(chns_Pyramid[2][c]);
					}
						lam_s=lam_s/(size1*6.0);
						lam_ss=lam_ss/(size2*6.0);
						lam_tmp=-cv::log(lam_ss.val[0]/lam_s.val[0])/cv::log(scales[real_scal[2]]/scales[real_scal[1]]);
				   for (int c=4;c<10;c++)
				   {
				      lambdas.push_back(lam_tmp);
				   }
		}else{
			//compute lambdas
			double size0,size1,lam_tmp;
			size0 =(double)chns_Pyramid[0][0].rows*chns_Pyramid[0][0].cols;
			size1 =(double)chns_Pyramid[1][0].rows*chns_Pyramid[1][0].cols;
			//compute luv	 
			for (int c=0;c<3;c++)
			{
				lam_s+=sum(chns_Pyramid[0][c]);		
				lam_ss+=sum(chns_Pyramid[1][c]);
			}
			lam_s=lam_s/(size0*3.0);
			lam_ss=lam_ss/(size1*3.0);
			lam_tmp=-cv::log(lam_ss.val[0]/lam_s.val[0])/cv::log(scales[real_scal[1]]/scales[real_scal[0]]);
			for (int c=0;c<3;c++)
			{
				lambdas.push_back(lam_tmp);
			}
			//compute  mag
			lam_s=sum(chns_Pyramid[0][4])/(size0*1.0);
			lam_ss=sum(chns_Pyramid[1][4])/(size1*1.0);
			lambdas.push_back(-cv::log(lam_ss.val[0]/lam_s.val[0])/cv::log(scales[real_scal[1]]/scales[real_scal[0]]));
			//compute grad_hist
			for (int c=4;c<10;c++)
			{
				lam_s+=sum(chns_Pyramid[0][c]);		
				lam_ss+=sum(chns_Pyramid[1][c]);
			}
			lam_s=lam_s/(size0*6.0);
			lam_ss=lam_ss/(size1*6.0);
			lam_tmp=-cv::log(lam_ss.val[0]/lam_s.val[0])/cv::log(scales[real_scal[1]]/scales[real_scal[0]]);
			for (int c=4;c<10;c++)
			{
				lambdas.push_back(lam_tmp);
			}
		}
	}else{
		lambdas.resize(10);
		for(int n=0;n<3;n++)
		{
		 lambdas[n]=lam[0];
		}
		 lambdas[3]=lam[1];
		for(int n=4;n<10;n++)
		{
		 lambdas[n]=lam[2];
		}	
	}
		//compute approx 特征基准
		vector<int> approx_scal;
		for (int s_r=0;s_r<nscales;s_r++)
		{
			int tmp=s_r/(nApprox+1);
			if (s_r-real_scal[tmp]>((nApprox+1)/2))
			{
				approx_scal.push_back(real_scal[tmp+1]);
			}else{
				approx_scal.push_back(real_scal[tmp]);
			}	
		}

		//compute approxPyramid
		double ratio;
		for (int ap_id=0;ap_id<(int)approx_scal.size();ap_id++)
		{
			vector<Mat> approx_chns;
			approx_chns.clear();
			/*memory is consistent*/
			int approx_rows=ap_size[ap_id].height;
			int approx_cols=ap_size[ap_id].width;
			Mat approx=Mat::zeros(10*approx_rows,approx_cols,CV_32FC1);//因为chns_Pyramind是32F
			for(int n_chans=0;n_chans<chns_num;n_chans++)
			{
				Mat py=approx.rowRange(n_chans*approx_rows,(n_chans+1)*approx_rows);
				int ma=approx_scal[ap_id]/(nApprox+1);
				resize(chns_Pyramid[ma][n_chans],py,py.size(),0.0,0.0,1);
				ratio=(double)pow(scales[ap_id]/scales[approx_scal[ap_id]],-lambdas[n_chans]);
				py=py*ratio;
				//smooth channels, optionally pad and concatenate channels
				convTri(py,py,Km);
				approx_chns.push_back(py);
			} 
			/*  check the pointer */
			/*	float *add1 = (float*)approx_chns[0].data;
				for( int c=1;c<approx_chns.size();c++)
				{
					cout<<"pointer "<<static_cast<void*>(approx_chns[c].data)<<" "<< (static_cast<void*>(add1+c*ap_size[ap_id].height*ap_size[ap_id].width))<<endl;
				cout<<"pointer "<<(static_cast<void*>(approx_chns[c].data) == (static_cast<void*>(add1+c*ap_size[ap_id].height*ap_size[ap_id].width))? true :false)<<endl;
				}*/
			approxPyramid.push_back(approx_chns);
		}
	delete []kern;
    vector<int>().swap(real_scal);
	vector<int>().swap(approx_scal);
	vector<double>().swap(lambdas);
	vector<vector<Mat> >().swap(chns_Pyramid);
}
void feature_Pyramids:: chnsPyramid(const Mat &img,  vector<vector<Mat> > &chns_Pyramid,vector<double> &scales) const//nApprox==0时
{
	int nPerOct =m_opt.nPerOct;
	int nOctUp =m_opt.nOctUp;
	int shrink =m_opt.shrink;
	int smooth =m_opt.smooth;
	int nbins=m_opt.nbins;
	int binsize=m_opt.binsize;
	int nApprox=m_opt.nApprox;
	Size diam =m_opt.minDS;
	/*get scales*/
	double minDs=(double)min(diam.width,diam.height);
	int nscales=(int)floor(nPerOct*(nOctUp+log(min(img.cols/minDs,img.rows/minDs))/log(2))+1);
	vector<Size> ap_size;
	Size ap_tmp_size;

	CV_Assert(nApprox<nscales);
	/*compute real & approx scales*/
	//vector<float> scales;
	chns_Pyramid.clear();
	scales.clear();
	double d0=(double)min(img.rows,img.cols);
	double d1=(double)max(img.rows,img.cols);
	for (float s=0;s<nscales;s++)
	{
		/*adjust ap_size*/
		double sc=pow(2.0,(-s)/nPerOct+nOctUp);
		double s0=(cvRound(d0*sc/shrink)*shrink-0.25*shrink)/d0;
		double s1=(cvRound(d0*sc/shrink)*shrink+0.25*shrink)/d0;
		double ss,es1,es2,a=10,val;
		for(int c=0;c<101;c++)
		{
			ss=(double)((s1-s0)*c/101+s0);
			es1=abs(d0*ss-cvRound(d0*ss/shrink)*shrink);
			es2=abs(d1*ss-cvRound(d1*ss/shrink)*shrink);
			if (max(es1,es2)<a)
			{
				a=max(es1,es2);
				val=ss;
			}
		}
		if (scales.empty())
		{
			scales.push_back(val);
			/*save ap_size*/
			ap_tmp_size.height=cvRound((img.rows*val)/shrink);
			ap_tmp_size.width=cvRound((img.cols*val)/shrink);
			ap_size.push_back(ap_tmp_size);
			
		}else
		{
			if (val!=scales.back())
			{
				//printf("3d=%f\n",val);
				scales.push_back(val);
				/*save ap_size*/
				ap_tmp_size.height=cvRound((img.rows*val)/shrink);
				ap_tmp_size.width=cvRound((img.cols*val)/shrink);
				ap_size.push_back(ap_tmp_size);
			}
		}		
	}

	//compute the filter
	Mat Km;
	Mat img_tmp;
	double *kern=new double[2*m_opt.smooth+1];
	for (int cout=0;cout<=m_opt.smooth;cout++)
	{
		kern[cout]=(double)((cout+1)/((m_opt.smooth+1.0)*(m_opt.smooth+1.0)));
		kern[2*m_opt.smooth-cout]=kern[cout];
	}
	Km=Mat(1,(2*m_opt.smooth+1),CV_64FC1,kern); 
	convTri(img,img_tmp,Km);

	//compute real 
	for (int s_r=0;s_r<(int)scales.size();s_r++)
	{
		vector<Mat> chns;
		resize(img_tmp,img_tmp,ap_size[s_r]*shrink,0.0,0.0,1);
		computeChannels(img_tmp,chns);
		chns_Pyramid.push_back(chns);
	}
	
	delete []kern;
	vector<Size>().swap(ap_size) ;
}
void feature_Pyramids::setParas(const detector_opt &in_para)
{
     m_opt=in_para;
}
void feature_Pyramids::compute_lambdas(const vector<Mat> &fold)
{
    cout<<"computing ...lambdas "<<endl;
	/*get the Pyramid*/
	int nimages=fold.size();//the number of the images used to be train,must>=2;
	Mat image;
	CV_Assert(nimages>1);
	//配置参数
	feature_Pyramids feature_set;
	detector_opt in_opt;
	in_opt.nApprox=0;
	feature_set.setParas(in_opt);
	vector<vector<vector<Mat> > > Pyramid_set;
	vector<double> scal;
	for (int n=0;n<nimages;n++)
	{
		image= fold[n];
		vector<vector<Mat> > Pyramid;
		feature_set.chnsPyramid(image,Pyramid,scal);
		Pyramid_set.push_back(Pyramid);
	}
	vector<double> mean;	
	mean.resize(3);
	vector<vector<double> >Pyramid_mean;
	vector<vector<vector<double> > > Pyramid_set_mean;
	/*compute the mean of the n_type,where n_type=3(color,mag,gradhist)*/
	for (int m=0;m<Pyramid_set.size();m++)//图像
	{
		Pyramid_mean.clear();
		for (int n=0;n<Pyramid_set[m].size();n++)//比例scales
		{
			double size=Pyramid_set[m][n][0].rows*Pyramid_set[m][n][0].cols*1.0;
			Scalar lam_color,lam_mag,lam_hist;
			for (int p=0;p<3;p++)
			{
				lam_color+=sum(Pyramid_set[m][n][p]);
			}
			mean[0]=lam_color[0]/(size*3.0);
			lam_mag=sum(Pyramid_set[m][n][3]);
			mean[1]=lam_mag[0]/(size*1.0);

			for (int p = 0; p < 6; p++)
			{
				lam_hist+=sum(Pyramid_set[m][n][p+4]);
			}
			mean[2]=lam_hist[0]/(size*6.0);

			Pyramid_mean.push_back(mean);
		}
		Pyramid_set_mean.push_back(Pyramid_mean);
	}
	// run

#ifdef test
	Mat matlab(36,3,CV_32FC1);
	LoadData("b.txt",matlab,36,3,1);
	double meantmp;
	for (int m=0;m<Pyramid_set.size();m++)//图像
	{

		Pyramid_mean.clear();
		for (int n=0;n<Pyramid_set[m].size();n++)//比例scales
		{

			//meantmp=matlab.at<float>(n,0);
			mean[0]=matlab.at<float>(n,0);
			mean[1]=matlab.at<float>(n,1);
			mean[2]=matlab.at<float>(n,2);
			Pyramid_mean.push_back(mean);
		}
		Pyramid_set_mean.push_back(Pyramid_mean);
	}
#endif // test
	/*remove the small value when scale==1*/
	vector<vector<double> > base_data;
	base_data.resize(3);
	for (int i=0;i<3;i++)
	{
		for (int m=0;m<nimages;m++)
		{
			base_data[i].resize(nimages);
			base_data[i][m]=Pyramid_set_mean[m][0][i];
		}
	}

	double  maxdata0= *max_element( base_data[0].begin(),base_data[0].end());
	double  maxdata1= *max_element( base_data[1].begin(), base_data[1].end());
	double  maxdata2= *max_element( base_data[2].begin(), base_data[2].end());

	for (int n=0;n<nimages;n++)
	{
		if (base_data[0][n]<maxdata0/50.0 ) base_data[0][n]=0;
		if (base_data[1][n]<maxdata1/50.0 ) base_data[1][n]=0;
		if (base_data[2][n]<maxdata2/50.0 ) base_data[2][n]=0;						
	}
	/*get the lambdas*/
	//1.compute mus
	double num=0;//有效的图像数量
	Mat mus=Mat::zeros(scal.size()-1,3,CV_64FC1);//
	Mat s=Mat::ones(scal.size()-1,2,CV_64FC1);//0~35个log（scale）
	FileStorage s_fs("s.yml", FileStorage::WRITE);
	for (int r=0;r<scal.size()-1;r++)
	{
		s.at<double>(r,0)=log(scal[r+1])/log(2);
	}
	double sum;
	for (int m=0;m<scal.size()-1;m++)//m scales
	{
		for (int L=0;L<3;L++)//
		{
			num=0;
			sum=0;
			for (int n=0;n<nimages;n++)
			{
				if (base_data[L][n]!=0)//比较小的点就舍弃 该幅图像
				{	
					num++;
					sum+=(Pyramid_set_mean[n][m+1][L]/base_data[L][n]);//把nimages幅图像求平均
				}
			}
			mus.at<double>(m,L)=log(sum/num)/log(2);
		}
	}
	//compute lam;
	for (int n=0;n<3;n++)
	{
		Mat mus_omega=mus.colRange(n,n+1);
		Mat lam_omgea=(s.t()*s).inv()*(s.t())*mus_omega;
	    lam.push_back(-lam_omgea.at<double>(0,0));
        cout<<"lambdas :"<<lam[n]<<endl;
	}
    cout<<"computing lambda done"<<endl;
    
}
const detector_opt& feature_Pyramids::getParas() const
{
	 return m_opt;
}
feature_Pyramids::feature_Pyramids()
{
	
	m_opt = detector_opt();
}
feature_Pyramids::~feature_Pyramids()
{
}

