/*
 *  Copyright (c) 2023, MAP IV.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *  v1.0 amc-nu 2023-08
 */

#include <opencv2/core/mat.hpp>

#include "bag_to_image.hpp"
#include "rclcpp/rclcpp.hpp"

BagToImage::BagToImage(const rclcpp::NodeOptions &options)
    : Node("bag_to_image", options) {
  // IO
  // input_path_ = this->declare_parameter<std::string>("input/path");
  input_path_ = this->declare_parameter<std::string>("input/path", "");
  bag_format_ = this->declare_parameter<std::string>("input/bag_format", "cdr");
  storage_id_ =
      this->declare_parameter<std::string>("input/bag_storage_id", "sqlite3");
  RCLCPP_INFO_STREAM(get_logger(), "Using bag format: '" << bag_format_ << "'");
  RCLCPP_INFO_STREAM(get_logger(),
                     "Using bag storage_id: '" << storage_id_ << "'");

  // input_topics_ =
  // this->declare_parameter<std::vector<std::string>>("input/topics");
  std::vector<std::string> dummy;
  input_topics_ =
      this->declare_parameter<std::vector<std::string>>("input/topics", dummy);
  output_path_ = this->declare_parameter<std::string>("output/path", "/tmp/");

  // input_topic_ = this->declare_parameter<std::string>("input/topic",
  // "/image_raw"); output_path_ =
  // this->declare_parameter<std::string>("output/path", "/tmp/");

  CheckParams();
  ReadBag();

  RCLCPP_INFO_STREAM(get_logger(), "Complete");
  rclcpp::shutdown();
}

void BagToImage::ReadBag() {
  rosbag2_cpp::StorageOptions storage_options;
  rosbag2_cpp::ConverterOptions converter_options;
  storage_options.uri = input_path_;
  storage_options.storage_id = storage_id_;
  converter_options.output_serialization_format = bag_format_;
  RCLCPP_INFO_STREAM(get_logger(), input_path_);
  RCLCPP_INFO_STREAM(get_logger(), storage_id_);
  RCLCPP_INFO_STREAM(get_logger(), bag_format_);
  RCLCPP_INFO_STREAM(get_logger(), output_path_);

  CreateDirectories();

  rosbag2_cpp::Reader reader(
      std::make_unique<rosbag2_cpp::readers::SequentialReader>());
  reader.open(storage_options, converter_options);

  rosbag2_storage::StorageFilter storage_filter;
  storage_filter.topics = input_topics_;
  reader.set_filter(storage_filter);

  auto topics_types = reader.get_all_topics_and_types();

  while (reader.has_next()) {
    auto bag_message = reader.read_next();

    if (std::find(input_topics_.begin(), input_topics_.end(),
                  bag_message->topic_name) != input_topics_.end()) {
      std::string curr_topic_type;
      for (const auto &topic_type : topics_types) {
        if (topic_type.name == bag_message->topic_name) {
          curr_topic_type = topic_type.type;
        }
      }
      if (curr_topic_type.empty()) {
        RCLCPP_WARN_STREAM(
            get_logger(),
            "No Image or CompressedImage topic types available in the rosbag");
        break;
      }

      auto image_msg = MessageToImage(bag_message, curr_topic_type);

      if (image_msg == nullptr) {
        RCLCPP_INFO_STREAM(get_logger(),
                           "Could not convert the message to Image type: "
                               << bag_message->topic_name << " at "
                               << bag_message->time_stamp);
        continue;
      }
      std::string fname;
      std::string target_topic_name = bag_message->topic_name;
      if (target_topic_name.substr(0, 1) == "/") {
        target_topic_name = target_topic_name.substr(1);
      }
      target_topic_name =
          std::regex_replace(target_topic_name, std::regex("/"), "_");
      // fname =
      //     output_path_ + "/" + target_topic_name + "/" + target_topic_name +
      //     "_" + boost::lexical_cast<std::string>(image_msg->header.stamp.sec)
      //     +
      //     "." +
      //     boost::lexical_cast<std::string>(image_msg->header.stamp.nanosec) +
      //     ".png";

      // To ensure nanosec is 9-digit by filling zeros at the front
      // checking
      // int counter;
      // if (target_topic_name == "pi4_2_D435_2_depth_image_rect_raw" &&
      //     image_msg->header.stamp.sec == 1713514760) {
      //   std::stringstream nanosec;
      //   nanosec << std::setw(9) << std::setfill('0')
      //      << image_msg->header.stamp.nanosec;
      //   std::cout << "counter " << counter << " | "
      //             << image_msg->header.stamp.sec << "." << nanosec.str()
      //             << std::endl;
      //   counter++;
      // }
      // implementation
      std::stringstream nanosec;
      nanosec << std::setw(9) << std::setfill('0')
         << image_msg->header.stamp.nanosec;
      fname = output_path_ + "/" + target_topic_name + "/" + target_topic_name +
              "_" +
              boost::lexical_cast<std::string>(image_msg->header.stamp.sec) +
              "." + nanosec.str() + ".png";
      // checking
      // if (target_topic_name == "pi4_2_D435_2_depth_image_rect_raw" &&
      //     image_msg->header.stamp.sec == 1713514760) {
      //   cv::imwrite(fname, image_msg->image);
      //   RCLCPP_INFO_STREAM(get_logger(), "Image saved to: " << fname);
      // }

      cv::imwrite(fname, image_msg->image);
      RCLCPP_INFO_STREAM(get_logger(), "Image saved to: " << fname);

    } // end check if topic matches
  } // end of rosbag reading
}

cv_bridge::CvImagePtr BagToImage::MessageToImage(
    std::shared_ptr<rosbag2_storage::SerializedBagMessage> bag_message,
    const std::string &topic_type) {
  cv_bridge::CvImagePtr in_image_ptr;

  if (topic_type == "sensor_msgs/msg/Image") {
    sensor_msgs::msg::Image extracted_msg;
    rclcpp::Serialization<sensor_msgs::msg::Image> serialization;
    rclcpp::SerializedMessage extracted_serialized_msg(
        *bag_message->serialized_data);
    serialization.deserialize_message(&extracted_serialized_msg,
                                      &extracted_msg);
    try {
      // in_image_ptr = cv_bridge::toCvCopy(extracted_msg,
      //                                    sensor_msgs::image_encodings::BGR8);
      // extracted_msg.encoding = "mono16";
      // in_image_ptr = cv_bridge::toCvCopy(extracted_msg,
      //                                    sensor_msgs::image_encodings::MONO8);
      if (extracted_msg.encoding == "8UC1") {
        extracted_msg.encoding = "mono8";
        in_image_ptr = cv_bridge::toCvCopy(extracted_msg,
                                           sensor_msgs::image_encodings::MONO8);

      } else if (extracted_msg.encoding == "16UC1") {
        extracted_msg.encoding = "mono16";
        in_image_ptr = cv_bridge::toCvCopy(
            extracted_msg, sensor_msgs::image_encodings::MONO16);

      } else
        in_image_ptr = cv_bridge::toCvCopy(extracted_msg,
                                           sensor_msgs::image_encodings::BGR8);
    } catch (cv_bridge::Exception &e) {
      RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
    }
  }
  if (topic_type == "sensor_msgs/msg/CompressedImage") {
    sensor_msgs::msg::CompressedImage extracted_msg;
    rclcpp::Serialization<sensor_msgs::msg::CompressedImage> serialization;
    rclcpp::SerializedMessage extracted_serialized_msg(
        *bag_message->serialized_data);
    serialization.deserialize_message(&extracted_serialized_msg,
                                      &extracted_msg);
    try {
      in_image_ptr = cv_bridge::toCvCopy(extracted_msg,
                                         sensor_msgs::image_encodings::BGR8);
    } catch (cv_bridge::Exception &e) {
      RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
    }
  }
  return in_image_ptr;
}

void BagToImage::CreateDirectories() {
  for (const auto &target_topic : input_topics_) {
    RCLCPP_INFO_STREAM(get_logger(), target_topic);

    rcpputils::fs::path o_dir(output_path_);
    auto target_topic_name = target_topic;
    if (target_topic_name.substr(0, 1) == "/") {
      target_topic_name = target_topic_name.substr(1);
    }
    target_topic_name =
        std::regex_replace(target_topic_name, std::regex("/"), "_");
    o_dir = o_dir / rcpputils::fs::path(target_topic_name);
    if (rcpputils::fs::create_directories(o_dir)) {
      std::cout << "created: " << o_dir << std::endl;
    }
  }
}

void BagToImage::CheckParams() {
  if (input_topics_.empty()) {
    RCLCPP_ERROR_STREAM(
        get_logger(),
        "At least one topic required, none provided. Terminating...");
    rclcpp::shutdown(nullptr, "Invalid Input Topic(s)");
  }
  if (input_path_.empty()) {
    RCLCPP_ERROR_STREAM(get_logger(), "No input path provided. Terminating...");
    rclcpp::shutdown(nullptr, "Invalid Rosbag Path");
  }
  if (output_path_.empty()) {
    RCLCPP_ERROR_STREAM(get_logger(),
                        "Invalid Path provided:" << output_path_
                                                 << ". Terminating...");
    rclcpp::shutdown(nullptr, "Invalid Output Path");
  }
  if (!std::filesystem::exists(output_path_)) {
    RCLCPP_INFO_STREAM(get_logger(),
                       "The Provided path ["
                           << output_path_
                           << "]doesn't exist. Trying to create.");
    if (std::filesystem::create_directories(output_path_)) {
      RCLCPP_INFO_STREAM(get_logger(),
                         "The Provided Path was created successfully.");
    } else {
      RCLCPP_ERROR_STREAM(get_logger(),
                          "Could not create the output directory: "
                              << output_path_ << ". Terminating");
      rclcpp::shutdown(nullptr, "Missing Permissions on the output path");
    }
  }
  RCLCPP_INFO_STREAM(get_logger(), "Saving Images PNGs to:" << output_path_);
}

#include <rclcpp_components/register_node_macro.hpp>

RCLCPP_COMPONENTS_REGISTER_NODE(BagToImage)
