#include <google/protobuf/util/time_util.h>
#include <iostream>
#include <thread>
#include <string>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include "client.h"

#include "sns.grpc.pb.h"
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;
using csce438::SNSService;
using csce438::Request;
using csce438::Reply;
using csce438::Message;



class Client : public IClient
{
    public:
        Client(const std::string& hname,
               const std::string& uname,
               const std::string& p)
            :hostname(hname), username(uname), port(p)
            {}
    protected:
        virtual int connectTo();
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();
    private:
        std::string hostname;
        std::string username;
        std::string port;
        
        // You can have an instance of the client stub
        // as a member variable.
        std::unique_ptr<SNSService::Stub> stub_;
};

int main(int argc, char** argv) {

    std::string hostname = "localhost";
    std::string username = "default";
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
        switch(opt) {
            case 'h':
                hostname = optarg;break;
            case 'u':
                username = optarg;break;
            case 'p':
                port = optarg;break;
            default:
                std::cerr << "Invalid Command Line Argument\n";
        }
    }

    Client myc(hostname, username, port);
    // You MUST invoke "run_client" function to start business logic
    myc.run_client();

    return 0;
}

int Client::connectTo()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to create a stub so that
    // you call service methods in the processCommand/porcessTimeline
    // functions. That is, the stub should be accessible when you want
    // to call any service methods in those functions.
    // I recommend you to have the stub as
    // a member variable in your own Client class.
    // Please refer to gRpc tutorial how to create a stub.
	// ------------------------------------------------------------
    stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(hostname + ":" + port, grpc::InsecureChannelCredentials())));
    
    ClientContext context;
    Request request;
    request.set_username(username);
    Reply reply;
    
    Status status = stub_->Login(&context, request, &reply);
    if (status.ok() && (reply.msg() == "Login successful" || reply.msg() == "Welcome back!")) {
        return 1;
    } else {
        std::cout << "STATUS: " << status.ok() << std::endl;
        std::cout << "Reply: " << reply.msg() << std::endl;
        return -1;
    } // else
    
    return 1; // return 1 if success, otherwise return -1
}

IReply Client::processCommand(std::string& input)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse the given input
    // command and create your own message so that you call an 
    // appropriate service method. The input command will be one
    // of the followings:
	//
	// FOLLOW <username>
	// UNFOLLOW <username>
	// LIST
    // TIMELINE
	//
	// ------------------------------------------------------------
	
    // ------------------------------------------------------------
	// GUIDE 2:
	// Then, you should create a variable of IReply structure
	// provided by the client.h and initialize it according to
	// the result. Finally you can finish this function by returning
    // the IReply.
	// ------------------------------------------------------------
    
	// ------------------------------------------------------------
    // HINT: How to set the IReply?
    // Suppose you have "Follow" service method for FOLLOW command,
    // IReply can be set as follow:
    // 
    //     // some codes for creating/initializing parameters for
    //     // service method
    //     IReply ire;
    //     grpc::Status status = stub_->Follow(&context, /* some parameters */);
    //     ire.grpc_status = status;
    //     if (status.ok()) {
    //         ire.comm_status = SUCCESS;
    //     } else {
    //         ire.comm_status = FAILURE_NOT_EXISTS;
    //     }
    //      
    //      return ire;
    // 
    // IMPORTANT: 
    // For the command "LIST", you should set both "all_users" and 
    // "following_users" member variable of IReply.
    // ------------------------------------------------------------
    IReply ire;
    ClientContext context;
    Status status;
    Request request;
    Reply reply;
    
    if (input.substr(0, input.find(" ")) == "FOLLOW") {
        //Set request
        request.set_username(username);
        std::string user_to_follow = input.substr(input.find(" ") + 1);
        request.add_arguments(user_to_follow);
        
        //RPC call and result
        status = stub_->Follow(&context, request, &reply);
        ire.grpc_status = status;
        std::string message = reply.msg();
        if (message == "Can't follow yourself") {
            ire.comm_status = FAILURE_ALREADY_EXISTS;
        } else if (message == "User to follow not found") {
            ire.comm_status = FAILURE_INVALID_USERNAME;
        } else if (message == "You already follow this user") {
            ire.comm_status = FAILURE_INVALID_USERNAME;
        } else if (message == "Follow succeeded") {
            ire.comm_status = SUCCESS;
        }
    } else if (input.substr(0, input.find(" ")) == "UNFOLLOW") {
        //Set request
        request.set_username(username);
        std::string user_to_unfollow = input.substr(input.find(" ") + 1);
        request.add_arguments(user_to_unfollow);
        
        //RPC call and result
        status = stub_->UnFollow(&context, request, &reply);
        ire.grpc_status = status;
        std::string message = reply.msg();
        if (message == "Can't unfollow yourself") {
            ire.comm_status = FAILURE_INVALID_USERNAME;
        } else if (message == "User to unfollow not found") {
            ire.comm_status = FAILURE_INVALID_USERNAME;
        } else if (message == "Unfollow succeeded") {
            ire.comm_status = SUCCESS;
        } else if (message == "User to unfollow was never followed") {
            ire.comm_status = FAILURE_NOT_EXISTS;
        }
    } else if (input.substr(0, input.find(" ")) == "LIST") {
        //Set request
        request.set_username(username);
        
        //RPC call and result
        status = stub_->List(&context, request, &reply);
        ire.grpc_status = status;
        std::string message = reply.msg();
        if (message == "User to list with not found") {
            ire.comm_status = FAILURE_INVALID_USERNAME;
        } else if (message == "List succeeded") {
            ire.comm_status = SUCCESS;
        } // else if
        
        for (std::string all_users_string : reply.all_users()) {
            ire.all_users.push_back(all_users_string);
        } // for
        
        for (std::string following_users_string : reply.following_users()) {
            ire.following_users.push_back(following_users_string);
        } // for
    } else if (input.substr(0, input.find(" ")) == "TIMELINE") {
        ire.grpc_status = Status::OK;
        ire.comm_status = SUCCESS;
    } else {
        // set to Failure_Invalid as it corresponds to invalid command
        ire.grpc_status = Status::CANCELLED;
        ire.comm_status = FAILURE_INVALID;
    }
    
    return ire;
}

void Client::processTimeline()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
	// ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
	// ------------------------------------------------------------
	
	
	// This will have double-threaded implementation. One thread for writing posts
	// and one threads for receiving messages from other users
	
	// Setup stub call
	ClientContext context;
	std::shared_ptr<ClientReaderWriter<Message, Message>> stream(stub_->Timeline(&context));
	
	// Create initial message to setup timeline with 20 initial posts from people you follow
	    // Either outside of writer thread or inside writer thread before its while loop
	    // EDIT: Do it outside so the server sends the posts into the stream faster 
	Message startTimeline;
	startTimeline.set_username(username);
	startTimeline.set_msg("Start timeline");
	google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
	timestamp->set_seconds(time(NULL));
	timestamp->set_nanos(0);
	startTimeline.set_allocated_timestamp(timestamp);
	stream->Write(startTimeline);
	
	// Code for writer thread and reader thread
	    // Writer thread prompts user for input via client.h getPostMessage()
	    // Reader thread recieves timeline messages to output from the server
	        // It should recieve the initial 20 posts plus any other posts from the stream
	    // Since you don't have separate thread functions, you will use lambda functions for threads
	        // Syntax:
	        /* std::thread writer([username, stream]() {
	            
	        });
	        */
	        /* std::thread reader([stream]() {
	            
	        });
	        */
    std::thread writer([stream](std::string username) {
        while (true) {
            Message inputpost;
            inputpost.set_username(username);
            inputpost.set_msg(getPostMessage());
            //std::cout << inputpost.msg();
            google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
            timestamp->set_seconds(time(NULL));
            timestamp->set_nanos(0);
            inputpost.set_allocated_timestamp(timestamp);
            stream->Write(inputpost);
        }
        stream->WritesDone();
    }, username);
    
    std::thread reader([stream]() {
        Message recievingpost;
        while (stream->Read(&recievingpost)) {
            std::time_t post_timestamp = google::protobuf::util::TimeUtil::TimestampToTimeT(recievingpost.timestamp());
            displayPostMessage(recievingpost.username(), recievingpost.msg(), post_timestamp);
        }
    });
    
    writer.join();
    reader.join();
}
