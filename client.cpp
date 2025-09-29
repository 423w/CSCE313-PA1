/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Wesley Hawkes
	UIN: 934001846
	Date: 9/26/25
*/
#include "common.h"
#include "FIFORequestChannel.h"
// add includes to support wait and mkdir
#include <sys/wait.h>
#include <sys/stat.h>

using namespace std;

// create global buff capacity var
int buffcap = MAX_MESSAGE;

// supposed to support directory creation so just make a helper func
void create_directory (string name) {
	struct stat st;
	if (stat(name.c_str(), &st) == -1) {
		mkdir(name.c_str(), 0755);
	}
}

// also must support dp requests
double get_data_point(FIFORequestChannel* channel, int person, double time, int ecgno) {
	datamsg dm(person, time, ecgno);
	channel->cwrite(&dm, sizeof(datamsg)); // question
	double reply;
	channel->cread(&reply, sizeof(double)); //answer
	return reply;
}

// assignmnt says to gather 1000 dp
 void gather_data(FIFORequestChannel* chan, int person) {
      create_directory("received");
      string filename = "received/x" + to_string(person) + ".csv";  // received/x1.csv
      ofstream outfile(filename);
      for (int i = 0; i < 1000; i++) {
          double time = i * 0.004; // 4ms
          double ecg1 = get_data_point(chan, person, time, 1);
          double ecg2 = get_data_point(chan, person, time, 2);
          outfile << time << "," << ecg1 << "," << ecg2 << endl;
      }
	  outfile.close();
	  cout << "Collected data for person " << person << " in file " << filename << endl;
	}

	// get file size before transferring
__int64_t get_file_size(FIFORequestChannel* channel, string& filename) {
	filemsg fm(0, 0);
	int len = sizeof(filemsg) + (filename.size() + 1);
	char* buf = new char[len];
	memcpy(buf, &fm, sizeof(filemsg));
	strcpy(buf + sizeof(filemsg), filename.c_str());
	channel->cwrite(buf, len); 
	__int64_t fs;
	channel->cread(&fs, sizeof(__int64_t));
	delete[] buf;
	return fs;
	}

// transfer file in chunks
void transfer_file(FIFORequestChannel* channel, string& filename) {
	create_directory("received");
	string local_filename = "received/" + filename;
	__int64_t fs = get_file_size(channel, filename);
	cout << "File size of " << filename << " is " << fs << " bytes" << endl;
	string output = "received/" + filename;
	ofstream outfile(output, ios::binary);
	__int64_t offset = 0;
	__int64_t remaining = fs;
	char* buffer = new char[buffcap];
	while (remaining > 0) {
		int chunk = min(remaining, (__int64_t) buffcap);
		filemsg fm(offset, chunk);
		int len = sizeof(filemsg) + (filename.size() + 1);
		char* buf = new char[len];
		memcpy(buf, &fm, sizeof(filemsg));
		strcpy(buf + sizeof(filemsg), filename.c_str());
		channel->cwrite(buf, len);
		int nbytes = channel->cread(buffer, chunk);
		outfile.write(buffer, nbytes);
		offset += nbytes;
		remaining -= nbytes;
		delete[] buf;
	}
	outfile.close();
	cout << "Received file " << filename << " from server" << endl;
}

// get new channel
string get_new_channel(FIFORequestChannel* channel) {
	MESSAGE_TYPE m = NEWCHANNEL_MSG;
	channel->cwrite(&m, sizeof(MESSAGE_TYPE));
	char name[100];
	channel->cread(name, 100);
	string new_channel_name(name);
	return string(new_channel_name);
}

// cleanup channels
void cleanup(FIFORequestChannel* channel) {
	MESSAGE_TYPE m = QUIT_MSG;
	channel->cwrite(&m, sizeof(MESSAGE_TYPE));
}

int main (int argc, char *argv[]) {
	// update as -1 instead of defaults to imply not set
	int opt;
	int p = -1;
	double t = -1.0;
	int e = -1;
	// check if c flag has been set
	bool cflag = false;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:cm:")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'c':
				cflag = true;
				break;
			case 'm':
				buffcap = atoi(optarg);
				break;
		}
	}
	// fork and exec
	pid_t pid = fork();
	if (pid == 0) {
		// child process
		char buffer_arg[20];
		sprintf(buffer_arg, "-m%d", buffcap);
		char server_path[] = "./server";
  		char* args[] = {server_path, buffer_arg, NULL};
		execvp(args[0], args);
		// exec should trigger here
		cout << "Failed to start server" << endl;
		exit(1);
	}
	else if (pid < 0) {
		cout << "Fork failed" << endl;
		exit(1);
	}
	else {
	// parent process - wait for server to start
	sleep(1);
	
	FIFORequestChannel* chan = new
	FIFORequestChannel("control",
	FIFORequestChannel::CLIENT_SIDE);
	FIFORequestChannel* working_chan = chan;

	if (cflag) {
		string new_channel_name = get_new_channel(chan);
		working_chan = new FIFORequestChannel(new_channel_name, FIFORequestChannel::CLIENT_SIDE);
		cout << "Created new channel: " << new_channel_name << endl;
		}

		if (p != -1 && t != -1.0 && e != -1) {
			double data = get_data_point(working_chan, p, t, e);
			cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << data << endl;
		}
		else if (p != -1) {
			gather_data(working_chan, p);
		}
		else if (filename != "") {
			transfer_file(working_chan, filename);
		}
		// cleanup
		if (working_chan != chan) {
			cleanup(working_chan);
			delete working_chan;
		}
		cleanup(chan);
		delete chan;

		int status;
		waitpid(pid, &status, 0);
	}
}
//     FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);
	
// 	// example

//     char buf[MAX_MESSAGE]; // 256
//     datamsg x(1, 0.0, 1);
	
// 	memcpy(buf, &x, sizeof(datamsg));
// 	chan.cwrite(buf, sizeof(datamsg)); // question
// 	double reply;
// 	chan.cread(&reply, sizeof(double)); //answer
// 	cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	
//     // sending a non-sense message, you need to change this
// 	filemsg fm(0, 0);
// 	string fname = "teslkansdlkjflasjdf.dat";
	
// 	int len = sizeof(filemsg) + (fname.size() + 1);
// 	char* buf2 = new char[len];
// 	memcpy(buf2, &fm, sizeof(filemsg));
// 	strcpy(buf2 + sizeof(filemsg), fname.c_str());
// 	chan.cwrite(buf2, len);  // I want the file length;

// 	delete[] buf2;
	
// 	// closing the channel    
//     MESSAGE_TYPE m = QUIT_MSG;
//     chan.cwrite(&m, sizeof(MESSAGE_TYPE));
// }
