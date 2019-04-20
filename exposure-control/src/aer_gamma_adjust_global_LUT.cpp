#include <string>
#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <opencv2/highgui/highgui.hpp>
#include <cv_bridge/cv_bridge.h>
#include <sstream>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <cstdlib>
#include <nodelet/nodelet.h>
#include <dynamic_reconfigure/BoolParameter.h>
#include <dynamic_reconfigure/DoubleParameter.h>
#include <dynamic_reconfigure/IntParameter.h>
#include <dynamic_reconfigure/StrParameter.h>
#include <dynamic_reconfigure/GroupState.h>
#include <dynamic_reconfigure/Reconfigure.h>
#include <dynamic_reconfigure/Config.h>
#include <cmath>
#include <iomanip>
#include <eigen3/Eigen/Eigenvalues>
#include <eigen3/Eigen/Dense>

//#include <timer.h>


bool check_rate = false;
double frame_rate_req = 10.0; // maximum 80 fps

// Define all the lookup tables globally
	// The first set of lookuptables for gamma correction
	cv::Mat lookUpTable_01(1, 256, CV_8U);
	cv::Mat lookUpTable_05(1, 256, CV_8U);
	cv::Mat lookUpTable_08(1, 256, CV_8U);
	cv::Mat lookUpTable_1(1, 256, CV_8U);
	cv::Mat lookUpTable_12(1, 256, CV_8U);
	cv::Mat lookUpTable_15(1, 256, CV_8U);
	cv::Mat lookUpTable_19(1, 256, CV_8U);

	// The second lookuptable is metric provided by Shim's 2014 paper
	cv::Mat lookUpTable_metric(1, 256, CV_8U);
	/*cv::Mat lookUpTable_met_05(1, 256, CV_8U);
	cv::Mat lookUpTable_met_08(1, 256, CV_8U);
	cv::Mat lookUpTable_met_1(1, 256, CV_8U);
	cv::Mat lookUpTable_met_12(1, 256, CV_8U);
	cv::Mat lookUpTable_met_15(1, 256, CV_8U);
	cv::Mat lookUpTable_met_19(1, 256, CV_8U);*/



class imageProcess {


 public:

	ros::NodeHandle nh;
        	//ros::Duration delay_time(0.5);
	int imageConverter() {
		//cv::namedWindow("view", CV_WINDOW_NORMAL); // comment in implement
		image_transport::ImageTransport it(nh);
		image_transport::Subscriber sub = it.subscribe("camera/image_raw", 1, imageProcess::imageCallback1);
		ros::spin();
		//cv::destroyWindow("view"); // comment in implementation
		return 0;
	}


	static void imageCallback1(const sensor_msgs::ImageConstPtr& msg)
	{

		if (check_rate){
			try
			{
				check_rate = false;
				cv::Mat image_current;
				imageProcess ip;
				cv::Mat image_capture;
				cv::Size size(612,512);
				image_capture = cv_bridge::toCvCopy(msg, "mono8")->image;
				cv::resize(image_capture, image_current, size);
				//image_capture = image_current;

				///////////////////////////////////////////////////////////////////////////////////////////////////////////
				// Call the image processing funciton here (i.e. the gamma processing), returning a float point gamma value
				///////////////////////////////////////////////////////////////////////////////////////////////////////////
                                
				// add more gamma value to approximate curve fit
				//double gamma[13]={1.0/1.9, 1.0/1.7, 1.0/1.5, 1.0/1.3, 1.0/1.1, 1.0/1.05 ,1.0, 1.05, 1.1, 1.3, 1.5, 1.7, 1.9};
			        //double metric[13];
				double gamma[7]={1.0/1.9, 1.0/1.5, 1.0/1.2 ,1.0, 1.2, 1.5, 1.9};
				double metric[7];
			        double max_metric;
			        double max_gamma,alpha, expNew, expCur, shutter_cur, shutter_new, gain_cur, gain_new,upper_shutter;
				double lower_shutter = 100.0; // adjust if necessary [unit: micro-second]
        			double kp=0.4; // contorl the speed to convergence
				double d = 0.1, R; // parameters used in the nonliear function in Shim's 2018 paper 				
				int gamma_index; // index to record the location of the optimum gamma value
				bool gain_flag = false;

				// calculate the upper limit of shutter speed [unit:microsecond]
				if ((1.0/frame_rate_req)*1000000.0 > 32754.0){        
					upper_shutter = 32754.0;
					}
				else{
					upper_shutter = round(1000000.0/frame_rate_req);
					}
				

				// loop to call image_gradient_gamma function to obtain image gradient of each gamma
				// manually adjust the possible gamma values and the number of gamma to use
				for (int i=0; i<7; ++i){
                                metric[i]= ip.image_gradient_gamma(image_current, i)/1000000; // passing the corresponding index
				//std::cout << "\nmetric " << i << " is:" <<metric[i] <<std::endl;
				std::cout << "  " <<metric[i] <<std::endl;
				}
                                
				// loop to find out the index that correslated to the optimum/maximum gamma value
				double temp = -1.0;				
				for(int i = 0; i < 7; i++){
					if (metric[i] > temp){
						temp = metric[i];
						gamma_index = i;
					} // end of if
				} // end of for loop 


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////  Curve Fitting  ///////////////////////////////////////////////


				// Call the curve fitting function to find out coefficient
				double * coeff_curve;
				coeff_curve = ip.curveFit( gamma, metric);				
	
				double coeff[6];
				for ( int i = 0; i < 6; i++ ) {
      					
					coeff[i] = *(coeff_curve+i);
					std::cout << "\ncoeff " << i << " is:" << coeff[i] << std::endl; // comment
   				}

				double metric_check = 0.0;

				max_gamma = ip.findRoots1(coeff, metric[gamma_index]); // calling function findRoots1 to find opt_gamma
				
				std::cout << "\nopt_gamma now is:  " << max_gamma << std::endl; 
				
				for (int i=0; i<6; i++) 
				{
					metric_check = metric_check + coeff[i] * pow(max_gamma,5-i);
				}
								

				std::cout << "metric_check = " << metric_check << std::endl;

				if (max_gamma < 1.0/1.9 || max_gamma > 1.9)	
				{
                                	// find out the optimum gamma value associated with highest image gradient
					max_gamma = gamma[gamma_index];
				}
				else if (metric[gamma_index] > metric_check)
				{
					max_gamma = gamma[gamma_index];
				}
				

				/* // The following block is used if the function findRoots1 is in double* type
				double * roots_adr;
				roots_adr = ip.findRoots1 (coeff);

				double roots[4];
				for ( int i = 0; i < 4; i++ ) {
      					
					roots[i] = *(roots_adr+i);
					std::cout << "\nroot " << i << " is:" << roots[i] << std::endl; // comment
   				}
				*/ // end of commented block: call function: double * findRoots1
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				

                               
				// alpha value refers to Shim's 2014 paper
				if (max_gamma < 1)
                                	//{alpha = 0.5;} //original paper used 0.5
					{alpha = 1;}
                                if (max_gamma >= 1)
					{alpha = 1;}
				
				ros::param::get("/camera/spinnaker_camera_nodelet/exposure_time", shutter_cur); // get the current shutter
				ros::param::get("/camera/spinnaker_camera_nodelet/gain", gain_cur); // get the current gain			
				
				shutter_cur = shutter_cur / 1000000; // unit from micro-second to second
				
				expCur = log2(7.84/( shutter_cur* pow(2,(gain_cur/6.0)) ) );				

				/* testing: using current shutter speed to determine the speed to change exposure value
				   This is achieved by calculating kp as 10^-5 * current shutter speed
				   The reasonale behind is becasue in the EV vs shutter speed plot, the slope at the lower bound is very steep
				   In order word, in a bright scene, a small change in shutter speed has a significant impact on EV since the
				   the shutter speed is small, and it is in the denominator, small change in denominator led to huge impact
				*/

				//kp = shutter_cur * 10; // e.g. shutter = 1000 microsec -> kp = 0.01; shutter=10000 microsec -> kp = 0.1
				
				// This update function was implemented in Shim's 2018 paper which is an update version of his 2014 paper
				R = d * tan( (2 - max_gamma) * atan2(1,d) - atan2(1,d) ) + 1;
				expNew = (1 + alpha * kp * (R-1)) * expCur;
 	
				// Shim's 2014 version of update function
				//expNew = (1+ kp * alpha * (1-max_gamma)) * expCur; 
			
				shutter_new = (7.84) * 1000000/(pow(2,expNew)); //[unit: micro-second]
				
				//std::cout << "\ngain: " << round(gain_cur) << "   shutter_new: "<< shutter_new << std::endl; //


				if (shutter_new > upper_shutter)
                		{
                    		gain_flag= true;
                    		shutter_new=upper_shutter;
                		}
                		else if (shutter_new<lower_shutter)
                		{
                    		gain_flag= true;
                    		shutter_new=lower_shutter;
		                }
                		else { gain_flag=false;}
                
                		if (gain_flag==true)
                		{
                		gain_new = 6.0*(expCur-expNew)+gain_cur;
				
                    			if (gain_new > 30)
                    			{gain_new = 30;}
                    			else if (gain_new < -10)
                    			{gain_new = -10;}
				gain_flag = false;                    		
				//gain_cur= gain_new;
                		}
                		else if  (shutter_new < upper_shutter && shutter_new > lower_shutter){
                    		gain_new = 0.0;
				//gain_flag = false;
                    		//gain_cur=gain_new;
                		}

				///////////////////////////// Comment or delete the following three lines in implementation ////////////////
				std::cout << "\ngain: " << round(gain_cur) << "   shutter_new: "<< shutter_new << std::endl;
				std::cout << "\ncurrent opt met: " << metric[gamma_index] << "; met at 1.0: " << metric[3]<<std::endl;

				std::cout << "max gamma: " << max_gamma << " ; gain_new: " << gain_new << "; shutter_cur: "<< shutter_cur << " ; shutter_new:  " << shutter_new << std::endl;

				///////////////////////////////////////////////////////////////////
				// Then call the function to determine what exposure time and gain to update
                                ///////////////////////////////////////////////////////////////////

				
                		ip.ChangeParam(shutter_new, gain_new); // may input the gain and exposure time update

				//cv::imshow("view", image_current); //comment this line in actual implementation
				//cv::waitKey(1); //comment this line in actual implementation
			}
			catch (cv_bridge::Exception& e)
			{
				ROS_ERROR("Could not convert from '%s' to 'mono8'.", msg->encoding.c_str());
			}
		}
		else
		{
			//ros::Timer timer1 = nh.createTimer(ros::Duration(0.5), imageProcess::timerCallback1);
			//ros::Duration(0.15).sleep(); // IMPORTANT: Use this line to add delay if necessary
			check_rate = true;
		}

	} // end of imageCallback1


	double image_gradient_gamma(cv::Mat src_img, int j)
	{

	// Accepting the raw image and the index of gamma value as input argument

	/*  // start of block comment

    		// The first lookuptable for gamma correction
    		cv::Mat lookUpTable(1, 256, CV_8U);
    		uchar* p = lookUpTable.ptr();
    
    		// The second lookuptable metric provided by Shim's 2014 paper
    		cv::Mat lookUpTable_metric(1,256,CV_8U);
    		uchar* q = lookUpTable_metric.ptr();
    		double sigma = 255.0 * 0.06; // The sigma value is used in Shim's 2014 paper as a activation threshhold, the paper used a value of 0.06    
    		double lamda = 1000.0; // The lamda value used in Shim's 2014 paper as a control parameter to adjust the mapping tendency (larger->steeper) 

    		for( int i = 0; i < 256; ++i)
    		{  p[i] = cv::saturate_cast<uchar>(pow(i / 255.0, gamma) * 255.0);
       
       		// The following if statement to create a lookup table based on the activation threshold value in Shim's 2014 paper
			if (i >= sigma){
				q[i] = 255 * ( ( log10( lamda * ((i-sigma)/255.0) + 1) ) / ( log10( lamda * ((255.0-sigma)/255.0) + 1) ) );
	    		}
			else{
				q[i] = 0;
			}
     
		} // end of for loop
	*/   // end of block comment


	    cv::Mat res = src_img.clone();
	    ///////////////////// The following computes the image gradient of the gamma-processed image /////////////////////
	    cv::Mat grad_x, grad_y;
	    cv::Mat abs_grad_x, abs_grad_y, dst_img;
	    

		// Using the corresponding index to find out the correct lookuptable to use
			if (j == 0){
				cv::LUT(src_img, lookUpTable_01, res);
			}
			else if (j == 1){
				cv::LUT(src_img, lookUpTable_05, res);
			}
			else if (j == 2){
				cv::LUT(src_img, lookUpTable_08, res);
			}
			else if (j == 3){
				cv::LUT(src_img, lookUpTable_1, res);
			}
			else if (j == 4){
				cv::LUT(src_img, lookUpTable_12, res);
			}
			else if (j == 5){
				cv::LUT(src_img, lookUpTable_15, res);
			}
			else if (j == 6){
				cv::LUT(src_img, lookUpTable_19, res);
			}


	    // Define variables that will be used in the sobel gradient determination function
	    int scale = 1;
	    int delta = 0;
	    int ddepth = CV_8U;
	
	    // Call the Sobel function to determine gradient image in x and y direction
	
	    // Gradient X
	    cv::Sobel(res, grad_x, ddepth, 1, 0, 3, scale, delta, cv::BORDER_DEFAULT);
	    cv::convertScaleAbs(grad_x, abs_grad_x);

	    // Gradient Y
	    cv::Sobel(res, grad_y, ddepth, 0, 1, 3, scale, delta, cv::BORDER_DEFAULT);
	    cv::convertScaleAbs(grad_y, abs_grad_y);

	    cv::addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0, dst_img);

	    ////////////////////// The following computes the gradient metric based on the gradient image ///////////////////////////
    
	    // Method 1: simple sumation of total gradient
	    //double metric= cv::sum(dst_img)[0]; // simple sum of total gradient as metric (Alternative 1 of the Shim's metric) 
	    
	    // Method 2: Shim's 2014 gradient metric function
	    // Using the metric equation given in Shim's 2014 paper
	    cv::LUT(dst_img, lookUpTable_metric, res);
	    double metric= cv::sum(res)[0];


	    //cv::imshow("image_to_show",dst_img); // comment later
	    return metric;

	} //end of image_gradient_gamma()


    void ChangeParam (double shutter_new, double gain_new) // may have input of the updated gain, exposure time settings
    {
        dynamic_reconfigure::ReconfigureRequest srv_req;
        dynamic_reconfigure::ReconfigureResponse srv_resp;
        dynamic_reconfigure::BoolParameter acq_fps_bool; //enable "acquisition_frame_rate_enable"
        dynamic_reconfigure::StrParameter gain_auto_str,exp_auto_str,wb_auto_str; // Auto gain, white balance and exposure off
        dynamic_reconfigure::DoubleParameter acq_fps_double, exp_time_double, gain_double, exp_auto_upper_double;
        dynamic_reconfigure::Config conf;

	// set constant frame rate
        acq_fps_bool.name = "acquisition_frame_rate_enable"; // maximum frame rate: 80 fps
        acq_fps_bool.value = true;
        conf.bools.push_back(acq_fps_bool);

	// trun off built-in auto exposure
        exp_auto_str.name = "exposure_auto"; // shut off auto exposure
        exp_auto_str.value = "Off";
        conf.strs.push_back(exp_auto_str);

	// turn off auto gain
        gain_auto_str.name = "auto_gain"; // shut off auto gain adjustment
        gain_auto_str.value = "Off";
        conf.strs.push_back(gain_auto_str);

	/*
        wb_auto_str.name = "auto_white_balance"; // shut off auto white balance
        wb_auto_str.value = "Off";
        conf.strs.push_back(wb_auto_str);
	*/

	// Set required frame rate
        acq_fps_double.name = "acquisition_frame_rate"; // maximum frame rate: 80 fps
        acq_fps_double.value = frame_rate_req; //change frame rate as needed
        conf.doubles.push_back(acq_fps_double);

	// Update exposure time
        exp_time_double.name = "exposure_time"; // Maximum range: 0 to 32754 [unit: micro-seconds]
        exp_time_double.value = shutter_new; ///////////////////////////// using function return value
        conf.doubles.push_back(exp_time_double);

	// Update gain
        gain_double.name = "gain"; // Maximum range: -10 to 30
        gain_double.value = gain_new; ///////////////////////////////////// using function return value
        conf.doubles.push_back(gain_double);

	// calculate and set highest possible exposure time (the camera has a limit of 32754 [unit: microsecond])
        exp_auto_upper_double.name = "auto_exposure_time_upper_limit";
	if ((1.0/frame_rate_req)*1000000.0 > 32754.0){        
		exp_auto_upper_double.value = 32754.0;
	}
	else{
		exp_auto_upper_double.value = 1000000.0/frame_rate_req;
	}
        conf.doubles.push_back(exp_auto_upper_double);

        srv_req.config = conf;

        ros::service::call("/camera/spinnaker_camera_nodelet/set_parameters",srv_req, srv_resp);
    }


	void generate_LUT (){
		imageProcess ip4;

		// Need to keep the same gamma values as in imageCallBack
		double gamma[7]={1.0/1.9, 1.0/1.5, 1.0/1.2 ,1.0, 1.2, 1.5, 1.9}; 		
		
		double sigma = 255.0 * 0.06; // The sigma value is used in Shim's 2014 paper as a activation threshhold, the paper used a value of 0.06    
    		double lamda = 1000.0; // The lamda value used in Shim's 2014 paper as a control parameter to adjust the mapping tendency (larger->steeper) 


		uchar* p;
		uchar* q = lookUpTable_metric.ptr();

		for (int j = 0; j < 7; j++){
			if (j == 0){
				p = lookUpTable_01.ptr();
			}
			else if (j == 1){
				p = lookUpTable_05.ptr();
			}
			else if (j == 2){
				p = lookUpTable_08.ptr();
			}
			else if (j == 3){
				p = lookUpTable_1.ptr();
			}
			else if (j == 4){
				p = lookUpTable_12.ptr();
			}
			else if (j == 5){
				p = lookUpTable_15.ptr();
			}
			else if (j == 6){
				p = lookUpTable_19.ptr();
			}
			
    			for( int i = 0; i < 256; ++i)
    			{  p[i] = cv::saturate_cast<uchar>(pow(i / 255.0, 1/gamma[j]) * 255.0);
       
       			// The following if statement to create a lookup table based on the activation threshold value in Shim's 2014 paper
				if (i >= sigma){
					q[i] = 255 * ( ( log10( lamda * ((i-sigma)/255.0) + 1) ) / ( log10( lamda * ((255.0-sigma)/255.0) + 1) ) );
	    			}
				else{
					q[i] = 0;
				}
     
			} // end of for loop with index i

		}// end of for loop with index j

	    	//std::cout << typeid(q[0]).name() << std::endl;
		//std::cout << "\n LUT_1.0: " << lookUpTable_1 << std::endl;	   	
		//std::cout << "\n LUT_1/1.9: " << lookUpTable_01 << std::endl;
		//std::cout << "\n LUT_1.9: " << lookUpTable_19 << std::endl;
		//std::cout <<LUT_seq[0][1] << std::endl;    		
	} // end of generate_LUT()




double  findRoots1 (double a[6], double check)
{
  static double roots1[4];
  double ad[5];
  double opt_gamma = 997.0, met_temp;
  ad[4] = 5 * a[0];
  ad[3] = 4 * a[1];
  ad[2] = 3 * a[2];
  ad[1] = 2 * a[3];
  ad[0] = a[4];
  
  Eigen::MatrixXd companion_mat (4, 4);

  for (int n = 0; n < 4; n++)
    {
      for (int m = 0; m < 4; m++)
	{

	  if (n == m + 1)
	    companion_mat (n, m) = 1.0;

	  if (m == 4 - 1)
	    companion_mat (n, m) = -ad[n] / ad[4];

	}
    }
  Eigen::MatrixXcd eig = companion_mat.eigenvalues ();
  for (int i = 0; i < 4; i++)
    {
	met_temp = 0.0; // met_temp is used to check if the root can return a larger metric      

	if (std::imag (eig (i)) == 0) // if statement to determine whether or not root is true
	{
		roots1[i] = std::real(eig (i));	  
	}
      else
	{
	  	roots1[i] = 1000; // if root is imaginary, assign an overshoot value
	}
    

	if ((roots1[i] < 2.0) && (roots1[i] > 0.5)) //check if the calculated root is within range (.5,2)
	{
		for (int j=0; j<6; j++) 
		{
			met_temp = met_temp + a[j] * pow(roots1[i],5-j);
		}

		if (met_temp > check) // if the root can return a metric that is greater than current metric
		{
			opt_gamma = roots1[i];
			std::cout << "\n in function maximum metric is: " << met_temp << "\n" << std::endl;
		}

	}

	
	std::cout << "eig(i) is: " << std::real(eig (i)) << "   ima: " << std::imag (eig (i)) << std::endl;
}
  

    

    return opt_gamma;

}



/*
double * findRoots (double a1[6])
{
  static double root[4];
  double a = 5 * a1[5];
  double b = 4 * a1[4];
  double c = 3 * a1[3];
  double d = 2 * a1[2];
  double e = a1[1];

  double p = ((8 * a * a) - (3 * b * b)) / (8 * a * a);
  double q =
    ((b * b * b - 4 * a * b * c) + (8 * a * a * d)) / (8 * a * a * a);

  double t0 = c * c - 3 * b * d + 12 * a * e;
  double t1 =
    2 * c * c * c - 9 * b * c * d + 27 * b * b * e + 27 * a * d * d -
    72 * a * c * e;
  double Q = cbrt (0.5 * (t1 + sqrt (t1 * t1 - 4 * t0 * t0 * t0)));
  double S = 0.5 * sqrt ((-2 * p / 3) + (1 / (3 * a)) * (Q + (t0 / Q)));

  root[0] = (-0.25 * b / a) - S + 0.5 * sqrt (-4 * S * S - 2 * p + q / S);
  root[1] = (-0.25 * b / a) - S - 0.5 * sqrt (-4 * S * S - 2 * p + q / S);
  root[2] = (-0.25 * b / a) - S + 0.5 * sqrt (-4 * S * S - 2 * p - q / S);
  root[3] = (-0.25 * b / a) - S - 0.5 * sqrt (-4 * S * S - 2 * p - q / S);

  return root;


}
*/


double * curveFit (double x[7], double y[7])

{ static double coff[6];
  int i, j, k, n, N;

  n = 5;
  
  Eigen::MatrixXd A(7,6);
  Eigen::MatrixXd b(7,1);
   
  for (i = 0; i <7; i++)
      for (j = 5; j>=0; j--)
	{A(i,5-j) = pow (x[i], j);}
    
  for (i = 0; i <7; i++)
   {  b(i,0) = y[i]; } 
  
  Eigen::MatrixXd A1 = A.transpose()*A;
  Eigen::MatrixXd b1 = A.transpose()*b; 
  //Eigen::MatrixXd Q =A1.colPivHouseholderQr().solve(b1);
  Eigen::MatrixXd Q =A1.inverse()*b1;

  for(i=0; i<n+1; i++)
  {  coff[i] = Q(i);
	//std::cout << "\nx is: " << x[i] << "Q is: " << coff[i] << std::endl;
}

  return coff;
   

}

/*
double * curveFit (double x[7], double y[7])

{ static double coff[6];
  int i, j, k, n, N;
  //cout.precision (4);		//set precision
  n = 5;
  N = 7;
  double X[2 * n + 1];		//Array that will store the values of sigma(xi),sigma(xi^2),sigma(xi^3)....sigma(xi^2n)
  for (i = 0; i < 2 * n + 1; i++)
    {
      X[i] = 0;
      for (j = 0; j < N; j++)
	X[i] = X[i] + pow (x[j], i);	//consecutive positions of the array will store N,sigma(xi),sigma(xi^2),sigma(xi^3)....sigma(xi^2n)
    }
  Eigen::VectorXd b(6);
   
  for (i = 0; i < n + 1; i++)
    {
      b(i) = 0;
      for (j = 0; j < N; j++)
	b(i) = b(i) + pow (x[j], i) * y[j];	//consecutive positions will store sigma(yi),sigma(xi*yi),sigma(xi^2*yi)...sigma(xi^n*yi)
    }
  Eigen::MatrixXd A(6,6);
  for (i = 0; i <= n; i++)
   for (j = 0; j <= n; j++)
      { A(i,j) = X[i + j]; }
  Eigen::VectorXd Q = A.colPivHouseholderQr().solve(b);

  for(i=0; i<n+1; i++)
  {  coff[i] = Q(i);}

  return coff;
   

}

*/


};// end of class imageProcess











int main(int argc, char **argv)
{
	ros::init(argc, argv, "image_capture");

	imageProcess im1;
	im1.generate_LUT();	
	im1.imageConverter();

	ros::spin();


	/*
	ros::NodeHandle nh;
	cv::namedWindow("view", CV_WINDOW_NORMAL); // comment in implement
	image_transport::ImageTransport it(nh);
	*/


	//image_transport::Subscriber sub = it.subscribe("camera/image_raw", 1, imageProcess::imageCallback1);

	//ros::Timer timer1 = nh.createTimer(ros::Duration(0.5), sub);

	//cv::Mat image_capture = cv::imwrite("home/capture.jpg",sub);
	//sub.shutdown();
	//ros::spin();
	//cv::destroyWindow("view"); // comment in implementation
}


