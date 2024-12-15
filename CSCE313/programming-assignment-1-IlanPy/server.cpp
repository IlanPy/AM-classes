#include <thread>
#include "FIFORequestChannel.h"

using namespace std;


int buffercapacity = MAX_MESSAGE;
char* buffer = NULL; // buffer used by the server, allocated in the main
int serverId = 0;
int nchannels = 0;
vector<string> all_data[NUM_PERSONS];


/* pre-declared because function signature required call in process_newchannel_request */
void handle_process_loop (FIFORequestChannel* _channel);

/* Handle New Channel request from client.*/
void process_newchannel_request (FIFORequestChannel* _channel) {
	nchannels++;
	string new_channel_name = "data" + to_string(nchannels) + "_";
	char buf[30];
	strcpy(buf, new_channel_name.c_str());
	_channel->cwrite(buf, new_channel_name.size()+1);

	FIFORequestChannel* data_channel = new FIFORequestChannel(new_channel_name, FIFORequestChannel::SERVER_SIDE);
	thread thread_for_client(handle_process_loop, data_channel);
	thread_for_client.detach();
}

/* Pre Load the data from the file of "person" and store in "alldata" vector*/
void populate_file_data (int person) {
	//cout << "populating for person " << person << endl;
	string filename = "BIMDC/" + to_string(person) + ".csv";
	char line[100];
	ifstream ifs(filename.c_str());
	if (ifs.fail()){
		EXITONERROR("Data file: " + filename + " does not exist in the BIMDC/ directory");
	}
	
	while (!ifs.eof()) {
		line[0] = 0;
		ifs.getline(line, 100);
		if (ifs.eof()) {
			break;
		}
		
		if (line[0]) {
			all_data[person-1].push_back(string(line));
		}
	}
}

/* Return data of specific person from preloaded "alldata" vector*/
double get_data_from_memory (int person, double seconds, int ecgno) {
	int index = (int) round(seconds / 0.004);
	string line = all_data[person-1][index]; 
	vector<string> parts = split(line, ',');
	
	double ecg1 = stod(parts[1]);
	double ecg2 = stod(parts[2]); 
	if (ecgno == 1) {
		return ecg1;
	}
	else {
		return ecg2;
	}
}

/* Return chunk of file based on request offset and length*/
void process_file_request (FIFORequestChannel* rc, char* request) {
	filemsg f = *((filemsg*) request);
	string filename = request + sizeof(filemsg);
	filename = "BIMDC/" + filename; // adding the path prefix to the requested file name
	//cout << "Server received request for file " << filename << endl;

	if (f.offset == 0 && f.length == 0) { // means that the client is asking for file size
		__int64_t fs = get_file_size (filename);
		rc->cwrite ((char*) &fs, sizeof(__int64_t));
		return;
	}

	/* request buffer can be used for response buffer, because everything necessary have
	been copied over to filemsg f and filename*/
	char* response = request; 

	// make sure that client is not requesting too big a chunk
	if (f.length > buffercapacity) {
		cerr << "Client is requesting a chunk bigger than server's capacity" << endl;
		cerr << "Returning nothing (i.e., 0 bytes) in response" << endl;
		rc->cwrite(response, 0);
	}

	FILE* fp = fopen(filename.c_str(), "rb");
	if (!fp) {
		cerr << "Server received request for file: " << filename << " which cannot be opened" << endl;
		rc->cwrite(buffer, 0);
		return;
	}
	fseek(fp, f.offset, SEEK_SET);
	int nbytes = fread(response, 1, f.length, fp);

	/* making sure that the client is asking for the right # of bytes,
	this is especially imp for the last chunk of a file when the 
	remaining lenght is < buffercap of the client*/
	assert(nbytes == f.length); 

	rc->cwrite(response, nbytes);
	fclose(fp);
}

/* Process single data request and return a server object */
void process_data_request (FIFORequestChannel* rc, char* request) {
	datamsg* d = (datamsg*) request;
	double data = get_data_from_memory(d->person, d->seconds, d->ecgno);
	serverresponse resp(serverId, data);
	rc->cwrite(&resp, sizeof(serverresponse));
}

void process_unknown_request (FIFORequestChannel* rc) {
	char a = 0;
	rc->cwrite(&a, sizeof(char));
}


/* Process request based on message type */
void process_request (FIFORequestChannel *rc, char* _request) {
	MESSAGE_TYPE m = *((MESSAGE_TYPE*) _request);
	if (m == DATA_MSG) {
		usleep(rand() % 5000);
		process_data_request(rc, _request);
	}
	else if (m == FILE_MSG) {
		process_file_request(rc, _request);
	}
	else if (m == NEWCHANNEL_MSG) {
		process_newchannel_request(rc);
	}
	else {
		process_unknown_request(rc);
	}
}

/* Loop infinitely to read messages from FIFOChannel and process the requests */
void handle_process_loop (FIFORequestChannel *channel) {
	/* creating a buffer per client to process incoming requests
	and prepare a response */
	char* buffer = new char[buffercapacity];
	if (!buffer) {
		EXITONERROR ("Cannot allocate memory for server buffer");
	}

	while (true) {
		int nbytes = channel->cread(buffer, buffercapacity);
		if (nbytes < 0) {
			cerr << "Client-side terminated abnormally" << endl;
			break;
		}
		else if (nbytes == 0) {
			cout << "Server could not read anything... Terminating" << endl;
			break;
		}

		MESSAGE_TYPE m = *((MESSAGE_TYPE*) buffer);
		if (m == QUIT_MSG) {  // note that QUIT_MSG does not get a reply from the server
			cout << "Client-side is done and exited" << endl;
			break;
		}
		process_request(channel, buffer);
	}
	delete[] buffer;
	delete channel;
}

int main (int argc, char *argv[]) {
	buffercapacity = MAX_MESSAGE;
	int opt;

	while ((opt = getopt(argc, argv, "m:s:")) != -1) {
		switch (opt) {
			case 'm':
				buffercapacity = atoi(optarg);
				break;
			case 's':
				serverId = atoi(optarg);
				break;
		}
	}

	srand(time_t(NULL));
	for (int i = 0; i < NUM_PERSONS; i++) {
		populate_file_data(i+1);
	}
	
	//Create a FIFOChannel on Serverside with SERVER ID embeded in the name of the object
	FIFORequestChannel* control_channel = new FIFORequestChannel("control_"+to_string(serverId)+"_", FIFORequestChannel::SERVER_SIDE);
	handle_process_loop(control_channel);
	cout << "Server terminated" << endl;
}