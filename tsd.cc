#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Message;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;

/* Global variables */
struct User {
  std::string username;
  std::vector<std::string> following_users;
  bool active;
  ServerReaderWriter<Message, Message>* stream = 0;
  int timeline_file_lines = 0;
  //vector of username strings to track followed users
};

// TIMELINEEDIT
struct Post {
  std::string post_sender;
  std::string post_content;
  std::string post_time;
  std::time_t post_time_t;
};

std::vector<User> users;
std::unordered_map<std::string, int> user_index_map;

/* Class definition */
class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // LIST request from the user. Ensure that both the fields
    // all_users & following_users are populated
    // ------------------------------------------------------------
    //std::cout << "List reached" << std::endl;
    std::string username = request->username();
    
    if (user_index_map.find(username) == user_index_map.end()) {
      reply->set_msg("User to list with not found");
      reply->add_all_users("");
      reply->add_following_users("");
      return Status::OK;
    }
    
    for (User u : users) {
      //std::cout << u.username << ", ";
      reply->add_all_users(u.username);
    }
    
    int index = user_index_map[username];
    for (std::string name : users.at(index).following_users) {
      reply->add_following_users(name);
    }
    
    reply->set_msg("List succeeded");
    
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to follow one of the existing
    // users
    // ------------------------------------------------------------
    std::string username = request->username();
    std::string user_to_follow = request->arguments(0);
    if (username == user_to_follow) {
      reply->set_msg("Can't follow yourself");
      return Status::OK;
    } // if
    
    if (user_index_map.find(user_to_follow) == user_index_map.end()) {
      reply->set_msg("User to follow not found");
      return Status::OK;
    } 
    
    int index = user_index_map[user_to_follow];
    for (std::string name : users.at(index).following_users) {
      if (username == name) {
        reply->set_msg("You already follow this user");
        return Status::OK;
      }
    }
    
    // Handle successful follow
    users.at(index).following_users.push_back(username);
    std::ofstream outputfile;
    outputfile.open("./data/users/" + user_to_follow + ".txt", std::ios_base::app);
    if (!(outputfile.is_open())) {
      std::cout << "ERROR: Cannot write to ./data/users/" << user_to_follow << ".txt" << std::endl;
    } else {
      outputfile << username << "\n";
      outputfile.close();
    } // else
    reply->set_msg("Follow succeeded");
    
    return Status::OK; 
  } // Follow

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to unfollow one of his/her existing
    // followers
    // ------------------------------------------------------------
    
    // Parse request
    std::string username = request->username();
    std::string user_to_unfollow = request->arguments(0);
    
    // Begin error checking
    if (username == user_to_unfollow) {
      reply->set_msg("Can't unfollow yourself");
      return Status::OK;
    } // if
    
    if (user_index_map.find(user_to_unfollow) == user_index_map.end()) {
      reply->set_msg("User to unfollow not found");
      return Status::OK;
    } // if
    
    bool found_the_user = false;
    int users_index = user_index_map[user_to_unfollow];
    int users_size = users.at(users_index).following_users.size();
    int following_users_index = 0;
    for (int i = 0; i < users_size; i++) {
      if (username == users.at(users_index).following_users.at(i)) {
        found_the_user = true;
        following_users_index = i;
      } // if
    } // for
    
    // Handle successful unfollow and handle never followed case
    if (found_the_user == true) {
      users.at(users_index).following_users.erase(begin(users.at(users_index).following_users) + following_users_index);
      std::ofstream outputfile;
      outputfile.open("./data/users/" + user_to_unfollow + ".txt", std::ios::out | std::ios::trunc);
      if (!(outputfile.is_open())) {
        std::cout << "ERROR: Cannot write to ./data/users/" << user_to_unfollow << ".txt" << std::endl;
      } else {
        for (std::string name : users.at(users_index).following_users) {
          outputfile << name << "\n";
        }
        outputfile.close();
      } // else
      reply->set_msg("Unfollow succeeded");
    } else {
      reply->set_msg("User to unfollow was never followed");
    }
    
    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // a new user and verify if the username is available
    // or already taken
    // ------------------------------------------------------------
    //std::cout << "Reached Login" << std::endl;
    User u;
    std::string username = request->username();
    if (user_index_map.find(username) == user_index_map.end()) {
      //std::cout << "Reached if" << std::endl;
      // Handle if user has not logged in yet
      u.username = username;
      u.active = true;
      reply->set_msg("Login successful");
      // Write to users.txt
      std::ofstream outputfile;
      outputfile.open("./data/users.txt", std::ios_base::app);
      if (!(outputfile.is_open())) {
        std::cout << "ERROR: Cannot write to ./data/users.txt" << std::endl;
        return Status::CANCELLED;
      } // if
      outputfile << username << "\n";
      outputfile.close();
      // Write to username.txt
      outputfile.open("./data/users/" + username + ".txt");
      if (!(outputfile.is_open())) {
        std::cout << "ERROR: Cannot write to ./data/users/" << username << ".txt" << std::endl;
        return Status::CANCELLED;
      } // if
      outputfile << username << "\n";    // Testcases specify a user is itself one of the following_users
      outputfile.close();
      // Insert into info into vectors, Might need mutexes
      u.following_users.push_back(username);
      users.push_back(u);
      user_index_map[username] = users.size() - 1;
    } else {
      // std::cout << "Reached else" << std::endl;
      // Handle if user has logged in before
      int index = user_index_map[username];
      if (users.at(index).active) {
        // std::cout << "Reached else 1" << std::endl;
        reply->set_msg("Invalid username: User is already active");
      } else {
        // std::cout << "Reached else 2" << std::endl;
        reply->set_msg("Welcome back!");
        users.at(index).active = true;
        // EDIT: READ IN FROM THAT FILE RIGHT BEFORE OUTPUTTING 20 POSTS IN ANOTHER FUNCTION Read in from timeline file to update with new posts
      } // else
    } // else
    
    return Status::OK;
  }

  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // ------------------------------------------------------------
    
    // Has a format of while (stream->Read(&message)) {
    //                      stream->Write(message);
    //                 }
    
    // This function probably won't have threads, instead the Client processTimeline function will have threads to handle writing to
    // timeline and receiving messages from other users
    
    Message message;
    while (stream->Read(&message)) {
      if (message.msg() == "Start timeline") {
        int index = user_index_map[message.username()];
        users.at(index).stream = stream;
        
        // TIMELINE EDIT
        std::vector<Post> initial_posts;
        std::ifstream inputfile;
        inputfile.open("./data/timelines/" + message.username() + ".txt");
        std::string message_sender;
        std::string message_content;
        std::string message_time;
        std::time_t message_time_t;
        std::string line;
        int post_count = 1;
        int line_count = 1;
        while (inputfile.is_open() && !inputfile.eof()) {
          if (line_count == 1) {
            getline(inputfile, line);
            message_sender = line;
          } else if (line_count == 2) {
            getline(inputfile, line);
            message_content = line;
          } else if (line_count == 3) {
            inputfile >> message_time_t;
            getline(inputfile, line);
            //message_time = line;
          }
          
          if (line_count == 3) {
            if (users.at(index).timeline_file_lines > 20 && post_count <= (users.at(index).timeline_file_lines - 20)) {
              post_count++;
              line_count = 1;
            } else {
              Post initial_post;
              initial_post.post_sender = message_sender;
              initial_post.post_content = message_content;
              initial_post.post_time_t = message_time_t;
              initial_posts.insert(initial_posts.begin(), initial_post); //initial_posts.push_back(initial_post);
              //std::cout << message_sender << " " << message_content << " " << message_time_t << std::endl;
              line_count = 1;
            } // else
          } else {
            line_count++;
          } // else
        } // while
        inputfile.close();
        //Message initial_post_message;
        //Timestamp message_timestamp;
        for (int i = 0; i < initial_posts.size(); i++) {
          Message initial_post_message;
          //Timestamp* message_timestamp = new Timestamp();
          //std::cout << initial_posts.at(i).post_time_t << std::endl;
          //message_timestamp->set_seconds(initial_posts.at(i).post_time_t);
          //message_timestamp->set_nanos(0);
          Timestamp* message_timestamp = initial_post_message.mutable_timestamp();   //Timestamp* message_timestamp = new Timestamp();
          *message_timestamp = google::protobuf::util::TimeUtil::TimeTToTimestamp(initial_posts.at(i).post_time_t);
          //std::cout << initial_posts.at(i).post_time << "oo" << std::endl;
          //google::protobuf::util::TimeUtil::FromString(initial_posts.at(i).post_time, message_timestamp);
          //*message_timestamp = google::protobuf::util::TimeUtil::TimeTToTimestamp(initial_posts.at(i).post_time_t);
          initial_post_message.set_username(initial_posts.at(i).post_sender);
          initial_post_message.set_msg(initial_posts.at(i).post_content);
          //initial_post_message.set_allocated_timestamp(message_timestamp);
          //initial_post_message.release_timestamp();
          stream->Write(initial_post_message);
        } // for
        
        /*
        std::string line;
        std::vector<std::string> initial_posts;
        std::ifstream inputfile;
        inputfile.open("./data/timelines/" + message.username() + ".txt");
        int line_count = 1;
        while (getline(inputfile, line)) {
          if (users.at(index).timeline_file_lines > 20 && line_count <= (users.at(index).timeline_file_lines - 20)) {
            line_count++;
            continue;
          } // if
          initial_posts.push_back(line);
        } // while
        Message initial_post_message;
        std::istringstream message_stream;
        std::string token;
        int stream_count;
        // Create variables for message_sender, message_content, message_timestamp (use FromString in time.util.h)
        std::string message_sender;
        std::string message_content;
        std::string message_time;
        Timestamp message_timestamp;
        for (int i = 0; i < initial_posts.size(); i++) {
          message_stream.str(initial_posts.at(i));
          stream_count = 0;
          while (std::getline(message_stream, token, '|')) {
            stream_count++;
            if (stream_count == 1) {
              message_sender = token;
            } else if (stream_count == 2) {
              message_content = token;
            } else if (stream_count == 3) {
              message_time = token;
            } // else if
          } // while
          //message_stream >> message_sender >> message_content >> message_time;
          google::protobuf::util::TimeUtil::FromString(message_time, &message_timestamp);
          initial_post_message.set_username(message_sender);
          initial_post_message.set_msg(message_content);
          initial_post_message.set_allocated_timestamp(&message_timestamp);
          stream->Write(initial_post_message);
        } // for
        */
      } else {
        std::string message_username = message.username();
        std::string message_content = message.msg();
        Timestamp message_timestamp = message.timestamp();
        std::string message_time = google::protobuf::util::TimeUtil::ToString(message_timestamp);
        std::time_t message_time_t = google::protobuf::util::TimeUtil::TimestampToTimeT(message_timestamp);
        //std::string message_time_t_string(std::ctime(&message_time_t));
        //message_time_t_string = message_time_t_string.substr(0, message_time_t_string.size() - 1);
        
        //TIMELINE EDIT
        std::ofstream outputfile;
        outputfile.open("./data/timelines/" + message_username + ".txt", std::ios_base::app);
        outputfile << message_username << std::endl;
        outputfile << message_content;
        outputfile << message_time_t << std::endl;
        outputfile.close();
        int users_index = user_index_map[message.username()];
        users.at(users_index).timeline_file_lines = users.at(users_index).timeline_file_lines + 1;
        for (std::string name : users.at(users_index).following_users) {
          int following_user_index = user_index_map[name];
          if (name != message_username && users.at(following_user_index).stream != 0 && users.at(following_user_index).active == true) {
            users.at(following_user_index).stream->Write(message);
          } // if
          if (name != message_username) {
            std::ofstream followerfile;
            followerfile.open("./data/timelines/" + name + ".txt", std::ios_base::app);
            followerfile << message_username << std::endl;
            followerfile << message_content;
            followerfile << message_time_t << std::endl;
            followerfile.close();
            users.at(following_user_index).timeline_file_lines = users.at(following_user_index).timeline_file_lines + 1;        
          }
          /*
          std::ofstream followerfile;
          followerfile.open("./data/timelines/" + name + ".txt", std::ios_base::app);
          followerfile << message_username << std::endl;
          followerfile << message_content;
          followerfile << message_time_t << std::endl;
          followerfile.close();
          users.at(following_user_index).timeline_file_lines = users.at(following_user_index).timeline_file_lines + 1;
          */
        } // for
        
        /*
        std::string fileoutput = message_username + "|" + message_content + "|" + message_time;
        std::ofstream outputfile;
        outputfile.open("./data/timelines/" + message_username + ".txt", std::ios_base::app);
        outputfile << fileoutput << "\n";
        outputfile.close();
        int users_index = user_index_map[message.username()];
        users.at(users_index).timeline_file_lines = users.at(users_index).timeline_file_lines + 1;
        
        for (std::string name : users.at(users_index).following_users) {
          int following_user_index = user_index_map[name];
          if (users.at(following_user_index).stream != 0 && users.at(following_user_index).active == true) {
            users.at(following_user_index).stream->Write(message);
          } // if
          std::ofstream followerfile;
          followerfile.open("./data/timelines/" + name + ".txt", std::ios_base::app);
          followerfile << fileoutput << "\n";
          followerfile.close();
          users.at(following_user_index).timeline_file_lines = users.at(following_user_index).timeline_file_lines + 1;
        } // for
        */
      } // else
    } // while
    
    int disconnected_user_index = user_index_map[message.username()];
    users.at(disconnected_user_index).active = false;
    
    return Status::OK;
  }
  
  /* private:
    //std::vector<> VECTOR TO HOLD USER STRUCTS
    //USE USERNAME AND INDEX HASHMAP TO INDEX THE USERS INSTEAD OF USING BELOW FUNCTION
    std::vector<User> users;
    std::unordered_map<std::string, int> user_index_map;
  */

};

void recoverData() {
  std::ifstream usersfile;
  usersfile.open("./data/users.txt");
  if (!(usersfile.is_open())) {
    std::cout << "Could not open users.txt" << std::endl;
    return;
  } // if
  
  std::string line;
  while (getline(usersfile, line)) {
    User u;
    u.username = line;
    u.active = false;
    
    std::ifstream followingusersfile;
    followingusersfile.open("./data/users/" + u.username + ".txt");
    if (!(followingusersfile.is_open())) {
      std::cout << "Could not open ./data/users/" << u.username << ".txt" << std::endl;
      return;
    } // if
    
    std::string fileline;
    while (getline(followingusersfile, fileline)) {
      u.following_users.push_back(fileline);
    } // while
    followingusersfile.close();
    
    std::ifstream timelinefile;
    timelinefile.open("./data/timelines/" + u.username + ".txt");
    int count = 0;
    int post_num = 0;
    while (getline(timelinefile, fileline)) {
      count++;
      if (count == 3) {
        post_num++;
        count = 0;
      }
    }
    u.timeline_file_lines = post_num;
    timelinefile.close();
    
    users.push_back(u);
    user_index_map[line] = users.size() - 1;
  } // while
  usersfile.close();
  /*
  for (User u : users) {
    std::ifstream followingusersfile;
    followingusersfile.open("./data/users/" + u.username + ".txt");
    if (!(followingusersfile.is_open())) {
      std::cout << "Could not open ./data/users/" << u.username << ".txt" << std::endl;
      return;
    } // if
    
    std::string fileline;
    while (getline(followingusersfile, fileline)) {
      u.following_users.push_back(fileline);
    } // while
    followingusersfile.close();
    
    std::ifstream timelinefile;
    timelinefile.open("./data/timelines/" + u.username + ".txt");
    int count = 0;
    int post_num = 0;
    while (getline(timelinefile, fileline)) {
      count++;
      if (count == 3) {
        post_num++;
        count = 0;
      }
    }
    u.timeline_file_lines = post_num;
    timelinefile.close();
  } // for
  */
}

void RunServer(std::string port_no) {
  // ------------------------------------------------------------
  // In this function, you are to write code 
  // which would start the server, make it listen on a particular
  // port number.
  // ------------------------------------------------------------
  std::string server_address = "0.0.0.0:" + port_no;
  recoverData();
  SNSServiceImpl service;
  
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char** argv) {
  
  std::string port = "3010";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;
          break;
      default:
	         std::cerr << "Invalid Command Line Argument\n";
    }
  }
  RunServer(port);
  return 0;
}
