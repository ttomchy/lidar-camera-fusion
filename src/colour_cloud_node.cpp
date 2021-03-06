#include <cmath>

#include "ros/ros.h"
#include "sensor_msgs/Image.h"
#include "sensor_msgs/PointCloud2.h"

#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>

#include <pcl/conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <Eigen/Dense>



using namespace cv;

ros::Publisher cloud_pub;
ros::Publisher rimg_pub;
cv::Mat rect_view, map1, map2;
bool map_initialised = false;
Eigen::MatrixXd tmat;

pcl::PointCloud<pcl::PointXYZ>::Ptr laserCloudIn(new pcl::PointCloud<pcl::PointXYZ>());


void cameraCallback(const sensor_msgs::Image::ConstPtr& img)
{
  ROS_INFO("Got Image!");

  Size imageSize = Size(img->width,img->height);
  cv_bridge::CvImagePtr cv_cam = cv_bridge::toCvCopy(img, "8UC3");

  if (!map_initialised) {
    ROS_INFO("Initalising camera mapping.");

    float distMat[9] = {754.53892599834842f, 0.0f, 319.5f,
        0.0f, 754.53892599834842f, 239.5f,
        0.0f, 0.0f, 1.0f};

    float distCoef[5] = {5.2038044809064208e-03, 1.5288890999953295e-01, 0., 0., -1.7854072082302619e+00};

    Mat cameraMatrix, distCoeffs;
    cameraMatrix = Mat(3,3, CV_32F,distMat);
    distCoeffs = Mat(5,1, CV_32F,distCoef);

    initUndistortRectifyMap(cameraMatrix, distCoeffs, Mat(),
                getOptimalNewCameraMatrix(cameraMatrix, distCoeffs, imageSize, 1, imageSize, 0),
                imageSize, CV_16SC2, map1, map2);

    map_initialised = true;
  }

  remap(cv_cam->image, rect_view, map1, map2, INTER_LINEAR);

  /*
  // Render points on image
  for(unsigned int i=0; i<laserCloudIn->size(); i++) {
      Eigen::Vector3d point_3d;
      Eigen::Vector3d point_t3d;
      point_3d << laserCloudIn->at(i).x,
          -laserCloudIn->at(i).z,
          laserCloudIn->at(i).y;
      point_t3d = tmat * point_3d;
      if (point_t3d[2] > 0) {
          circle(rect_view, Point((int)(point_t3d[0]/point_t3d[2]) ,(int)(point_t3d[1]/point_t3d[2] )),3,
              Scalar(255-(25*laserCloudIn->at(i).y),0,0), 1);
      }
  }
  */
  sensor_msgs::ImagePtr msg = cv_bridge::CvImage(std_msgs::Header(), "rgb8", rect_view).toImageMsg();

  rimg_pub.publish(msg);

}

void lidarCallback(const sensor_msgs::PointCloud2::ConstPtr& scan)
{
  ROS_INFO("Got point cloud!");
  pcl::fromROSMsg(*scan, *laserCloudIn);

  pcl::PointCloud<pcl::PointXYZRGB>::Ptr colour_laser_cloud(new pcl::PointCloud<pcl::PointXYZRGB>());

    for(unsigned int i=0; i<laserCloudIn->size(); i++) {
        float x3d = laserCloudIn->at(i).x;
        float y3d = laserCloudIn->at(i).y;
        float z3d = laserCloudIn->at(i).z;

        Eigen::Vector3d point_3d;
        Eigen::Vector3d point_t3d;
        point_3d << x3d, -z3d, y3d;
        point_t3d = tmat * point_3d;

        pcl::PointXYZRGB p;
        p.x = x3d;
        p.y = y3d;
        p.z = z3d;
        p.r = 0;
        p.g = 0;
        p.b = 0;
        if (point_t3d[2] > 0) {
            int image_u = (int)(point_t3d[0]/point_t3d[2]);
            int image_v = (int)(point_t3d[1]/point_t3d[2]);

            if (0 <= image_u && image_u < rect_view.size().width &&
                0 <= image_v && image_v < rect_view.size().height) {
              Vec3b colour= rect_view.at<Vec3b>(Point(image_u, image_v));
              p.r = colour[0];
              p.g = colour[1];
              p.b = colour[2];
            }

        }
        colour_laser_cloud->push_back(p);
    }

  sensor_msgs::PointCloud2 scan_color = sensor_msgs::PointCloud2();
  pcl::toROSMsg(*colour_laser_cloud, scan_color);
  scan_color.header.frame_id = "velodyne";
  cloud_pub.publish(scan_color);
}


int main(int argc, char **argv)
{
 ros::init(argc, argv, "colour_cloud");
 
 tmat = Eigen::MatrixXd(3,3);
 tmat <<   754.53892599834842f, 0.0f,                319.5f,
           0.0f,                754.53892599834842f, 239.5f,
           0.0f,                0.0f,                1.0f;

 ros::NodeHandle n;

 ros::Subscriber sub1 = n.subscribe("/usb_cam/image_raw", 2, cameraCallback);
 ros::Subscriber sub2 = n.subscribe("/velodyne_points", 2, lidarCallback);

 cloud_pub = n.advertise<sensor_msgs::PointCloud2>("colour_cloud", 10);
 rimg_pub = n.advertise<sensor_msgs::Image>("rect_image", 10);
 ros::spin();

 return 0;
}

