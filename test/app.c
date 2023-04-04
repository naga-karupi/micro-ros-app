#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <std_msgs/msg/header.h>

#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define STRING_BUFFER_LEN 50

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc); vTaskDelete(NULL);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

rcl_publisher_t publisher;
rcl_subscription_t subscriber;

std_msgs__msg__Header incoming_msg;
std_msgs__msg__Header out_coming_msg;
std_msgs__msg__Header incoming_pong;

int device_id;
int seq_no;

void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
	RCLC_UNUSED(last_call_time);

	if (timer != NULL) {

		seq_no = rand();
		sprintf(out_coming_msg.frame_id.data, "%d_%d", seq_no, device_id);
		out_coming_msg.frame_id.size = strlen(out_coming_msg.frame_id.data);

		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		out_coming_msg.stamp.sec = ts.tv_sec;
		out_coming_msg.stamp.nanosec = ts.tv_nsec;

		rcl_publish(&publisher, (const void*)&out_coming_msg, NULL);
	}
}

void sub_callback(const void * msgin)
{
	const std_msgs__msg__Header * msg = (const std_msgs__msg__Header *)msgin;

	// Dont pong my own pings
	if(strcmp(out_coming_msg.frame_id.data, msg->frame_id.data) != 0){
	}
}


void appMain(void *argument)
{
	rcl_allocator_t allocator = rcl_get_default_allocator();
	rclc_support_t support;

	// create init_options
	RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

	// create node
	rcl_node_t node;
	RCCHECK(rclc_node_init_default(&node, "pubsub_node", "", &support));

	// Create a reliable ping publisher
	RCCHECK(rclc_publisher_init_default(&publisher, &node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Header), "/microROS/pub"));


	// Create a best effort ping subscriber
	RCCHECK(rclc_subscription_init_best_effort(&subscriber, &node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Header), "/microROS/sub"));

	// Create a 3 seconds ping timer timer,
	rcl_timer_t timer;
	RCCHECK(rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(2000), timer_callback));


	// Create executor
	rclc_executor_t executor;
	RCCHECK(rclc_executor_init(&executor, &support.context, 3, &allocator));
	RCCHECK(rclc_executor_add_timer(&executor, &timer));
	RCCHECK(rclc_executor_add_subscription(&executor, &subscriber, &incoming_msg,
		&sub_callback, ON_NEW_DATA));

	// Create and allocate the pingpong messages
	char out_coming_msg_buffer[STRING_BUFFER_LEN];
	out_coming_msg.frame_id.data = out_coming_msg_buffer;
	out_coming_msg.frame_id.capacity = STRING_BUFFER_LEN;

	char incoming_msg_buffer[STRING_BUFFER_LEN];
	incoming_msg.frame_id.data = incoming_msg_buffer;
	incoming_msg.frame_id.capacity = STRING_BUFFER_LEN;

	char incoming_pong_buffer[STRING_BUFFER_LEN];
	incoming_pong.frame_id.data = incoming_pong_buffer;
	incoming_pong.frame_id.capacity = STRING_BUFFER_LEN;

	device_id = rand();

	while(1){
		rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
		usleep(10000);
	}

	// Free resources
	RCCHECK(rcl_publisher_fini(&publisher, &node));
	RCCHECK(rcl_subscription_fini(&subscriber, &node));
	RCCHECK(rcl_node_fini(&node));
}
