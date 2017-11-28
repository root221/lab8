#include <iostream> 
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <ros/ros.h>
#include <string>
#include <math.h>
// PCL specific includes
#include <sensor_msgs/PointCloud2.h>
#include <nav_msgs/Path.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include "pcl_ros/point_cloud.h"
#include <pcl/registration/icp.h>
#include <pcl/common/common.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/filters/filter.h> //pcl-1.7/
#include <pcl/search/organized.h>
#include <pcl/search/kdtree.h>
#include <pcl/impl/point_types.hpp>


//using namespace cv;
using namespace std;
using namespace pcl;


typedef pcl::PointCloud<pcl::PointXYZRGB> PointCloudXYZRGB;
ros::Publisher pcl_model_t0_publisher;
ros::Publisher pcl_model_t1_publisher;
ros::Publisher pose_publisher;
ros::Publisher cloud_icp_align_object_publisher;

void  cloud_cb (const sensor_msgs::PointCloud2ConstPtr& input);
void  pose_publish (PointCloudXYZRGB::Ptr cloud_icp_align,PointCloudXYZRGB::Ptr cloud_t0, PointCloudXYZRGB::Ptr cloud_t1);
void icp_pose_estimation (PointCloudXYZRGB::Ptr cloud_t0, PointCloudXYZRGB::Ptr cloud_t1);
PointCloudXYZRGB::Ptr model_t0 (new PointCloudXYZRGB);
PointCloudXYZRGB::Ptr model_t1(new PointCloudXYZRGB);
PointCloudXYZRGB::Ptr model_icp_align (new PointCloudXYZRGB);

void  cloud_cb (const sensor_msgs::PointCloud2ConstPtr& input)
{	
	//Step 1: Convert pointcloud type from Ros pointcloud2  (input) to PCL pointCLoudXYZRGB (model_t1)

	pcl::fromROSMsg (*input, *model_t1); //convert from PointCloud2 to pcl point type
	//Step 2: Use function icp_pose_estimation() to register consequently moving object (model_t0, model_t1) 
	//Step 3: Use function pose_publish() to publish moving object and aligned object pointcloud
	if (model_t0->points.size() == model_t1->points.size()){
		icp_pose_estimation(model_t0,model_t1);
		pose_publish(model_icp_align,model_t0,model_t1);
 	}
	copyPointCloud(*model_t1,*model_t0);
	//Step 4: Update the current point (model_t0)

}   

void  pose_publish (PointCloudXYZRGB::Ptr cloud_icp_align,PointCloudXYZRGB::Ptr cloud_t0, PointCloudXYZRGB::Ptr cloud_t1){
	//Evaluation
	double average_cloud_t1_x=0;
	double average_cloud_t1_y=0;
	double average_cloud_t1_z=0;
	double average_cloud_icp_align_x=0;
	double average_cloud_icp_align_y=0;
	double average_cloud_icp_align_z=0;
	for(int i=0;i<cloud_t1->points.size();i++){
		average_cloud_t1_x+=cloud_t1->points[i].x;
		average_cloud_t1_y+=cloud_t1->points[i].y;
		average_cloud_t1_z+=cloud_t1->points[i].z;
	}
	average_cloud_t1_x /=cloud_t1->points.size();
	average_cloud_t1_y /=cloud_t1->points.size();
	average_cloud_t1_z /=cloud_t1->points.size();

	for(int i=0;i<cloud_icp_align->points.size();i++){
		average_cloud_icp_align_x+=cloud_icp_align->points[i].x;
		average_cloud_icp_align_y+=cloud_icp_align->points[i].y;
		average_cloud_icp_align_z+=cloud_icp_align->points[i].z;
	}
	for(int i=0;i<cloud_icp_align->points.size();i++){
		cloud_icp_align->points[i].r = 0;
	}
	average_cloud_icp_align_x /=cloud_icp_align->points.size();
	average_cloud_icp_align_y /=cloud_icp_align->points.size();
	average_cloud_icp_align_z /=cloud_icp_align->points.size();

	double x_error= fabs((average_cloud_t1_x-average_cloud_icp_align_x)/average_cloud_t1_x);
	double y_error= fabs((average_cloud_t1_y-average_cloud_icp_align_y)/average_cloud_t1_y);
	double z_error= fabs((average_cloud_t1_z-average_cloud_icp_align_z)/average_cloud_t1_z);
	printf("x_error: %3lf percent, y_error: %3lf percent, z_error: %3lf percent\n",x_error*100,y_error*100,z_error*100);


	cloud_t0->header.frame_id = "/camera_link";
 	pcl_model_t0_publisher.publish(model_t0);
	cloud_t1->header.frame_id = "/camera_link";
 	pcl_model_t1_publisher.publish(cloud_t1);
 	cloud_icp_align->header.frame_id = "/camera_link";
 	cloud_icp_align_object_publisher.publish(*cloud_icp_align);
	return;	
}




void icp_pose_estimation (PointCloudXYZRGB::Ptr cloud_t0, PointCloudXYZRGB::Ptr cloud_t1){
	pcl::IterativeClosestPoint<pcl::PointXYZRGB, pcl::PointXYZRGB> icp;
	pcl::search::KdTree<pcl::PointXYZRGB>::Ptr tree1 (new pcl::search::KdTree<pcl::PointXYZRGB>);
	pcl::search::KdTree<pcl::PointXYZRGB>::Ptr tree2 (new pcl::search::KdTree<pcl::PointXYZRGB>);
	///////////Exercise : Use pcl icp function to get transform matrix///////////////////

	tree1->setInputCloud(cloud_t0);																				//l::search::KdTree<pcl::PointXYZRGB>::Ptr tree2 (new pcl::search::KdTree<pcl::PointXYZRGB>);				//
	tree2->setInputCloud(cloud_t1);																			//
	icp.setSearchMethodSource(tree1);																			//
	icp.setSearchMethodTarget(tree2);																			//
	icp.setInputSource(cloud_t0);				// Set align model													//		
	icp.setInputTarget(cloud_t1);		// Set align target													//
	// Set the max correspondence distance to 5cm (e.g., correspondences with higher distances will be ignored)	//												//
	icp.setMaxCorrespondenceDistance(1500);																		//
	icp.setTransformationEpsilon(1e-10);	// Set the transformation epsilon (criterion 1)						//										//
	icp.setEuclideanFitnessEpsilon(0.1);	// Set the euclidean distance difference epsilon (criterion 2)																	//
	icp.setMaximumIterations(300);			// Set the maximum number of iterations (criterion 3)				//													//
	std::cout << icp.getFinalTransformation() << std::endl;			

	////////////////////////////////////////////////////////////////////////////////////
	icp.align(*model_icp_align);
}



int   main (int argc, char** argv)
{
	
    // Initialize ROS
     ros::init (argc, argv, "my_pcl_tutorial");
     ros::NodeHandle nh;   
     // Create a ROS subscriber for the input point cloud
     ros::Subscriber model_subscriber = nh.subscribe<sensor_msgs::PointCloud2> ("/camera_link/moving_object", 1, cloud_cb);
     // Create a ROS publisher for the output point cloud
     pcl_model_t0_publisher = nh.advertise<PointCloudXYZRGB> ("/camera_link/move_model_t0", 1);
     pcl_model_t1_publisher = nh.advertise<PointCloudXYZRGB> ("/camera_link/move_model_t1", 1);
     cloud_icp_align_object_publisher = nh.advertise<PointCloudXYZRGB> ("/camera_link/icp_align_model", 1);
     pose_publisher = nh.advertise<nav_msgs::Path> ("/camera_link/pose_estimation", 1);
     // Spin
     ros::spin ();
}
