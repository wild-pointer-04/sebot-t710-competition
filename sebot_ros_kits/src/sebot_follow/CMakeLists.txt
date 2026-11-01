cmake_minimum_required(VERSION 3.0.2)
project(sebot_follow)

find_package(catkin REQUIRED COMPONENTS
  rospy
)

catkin_package(
#  INCLUDE_DIRS include
#  LIBRARIES sebot_visual
#  CATKIN_DEPENDS rospy geometry_msgs
#  DEPENDS system_lib
)

include_directories(
# include
  ${catkin_INCLUDE_DIRS}
)


install(PROGRAMS
  scripts/sebot_follow.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)


