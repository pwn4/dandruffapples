/*
 * Sort files found inside the input dir by time frame, merge them into one file and write the result to output dir.
 *
 * Input Dir:
 * 				default: /tmp/input/
 * 				change value: pass "-in value" when calling on the command line
 * 				notes: only log files that contain "antix_log" in them will be read
 * Output Dir:
 * 				default: /tmp/output/
 * 				change value: pass "-out value" when calling on the command line
 * 				notes: will write a file containing "antix_log" and an additional debug.txt file showing the approximate layout if the DEBUG define is defined
 */

#include <iostream>
#include <vector>
#include <string>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <iterator>
#include <exception>
#include <fstream>
#include <cstdlib>

#include "../common/helper.h"
#include "../common/messagewriter.h"
#include "../common/messagereader.h"
#include "../common/types.h"

#include "../common/timestep.pb.h"
#include "../common/serverrobot.pb.h"
#include "../common/puckstack.pb.h"

using namespace std;

struct Log {
	int fd, logNum;
	bool doneReading;
	uint64_t onTimeFrame;
	MessageReader reader;

	Log(int fd_, int logNum_) :
		fd(fd_), logNum(logNum_), doneReading(false), onTimeFrame(ULONG_MAX), reader(
				fd_) {
	}
};



//get the time frame of a log with the minimum time frame
uint64_t getNextTimeFrame(vector<Log> &inputLogsReader)
{
	uint64_t min = ULONG_MAX;

	for (vector<Log>::iterator it = inputLogsReader.begin(); it
			!= inputLogsReader.end(); it++){
		if(!it->doneReading && min > it->onTimeFrame)
			min = it->onTimeFrame;
	}

	return min;
}

//read from input logs and sorting them into one log file by the time frame number
void sortMergeLog(vector<Log> &inputLogsReader, MessageWriter logWriter,
		ofstream &debug) {
	TimestepUpdate timeframe;
	PuckStack puckstack;
	ServerRobot serverrobot;
	MessageType type;
	int len;
	const void *buffer;
	uint64_t nextTimeFrame;
	int readingLogs = inputLogsReader.size();

	//loop while we are still reading at least one input log file
	while (readingLogs > 0) {

		//loop through all the open input files
		for (vector<Log>::iterator it = inputLogsReader.begin(); it
				!= inputLogsReader.end(); it++) {

			/*read only the logs that we can read ( ex. no EOF exception yet ),
			 * on the minumum time frame ( out of all the read files ) and
			 * it has itialized onTimeFrame number ( so we know which one it starts off at )*/
			while ((!it->doneReading && (it->onTimeFrame == nextTimeFrame))
					|| it->onTimeFrame == ULONG_MAX) {
				try {
					for (bool complete = false; !complete;)
						complete = it->reader.doRead(&type, &len, &buffer);
				} catch (exception& e) {
					//if we encounter an exception while reading a log ( ex. EOF ) then stop reading it
#ifdef DEBUG
					debug << "log: " << it->logNum << ", Caught exception: "
							<< e.what() << endl;
#endif
					it->doneReading = true;
					readingLogs--;

					break;
				}

				if (type == MSG_SERVERROBOT) {
					serverrobot.ParseFromArray(buffer, len);
#ifdef DEBUG
					debug << "log: " << it->logNum << ", serverrobot id: "
							<< serverrobot.id() << endl;
#endif
					logWriter.init(MSG_SERVERROBOT, serverrobot);

					for (bool complete = false; !complete;)
						complete = logWriter.doWrite();
				} else if (type == MSG_PUCKSTACK) {
					puckstack.ParseFromArray(buffer, len);
#ifdef DEBUG
					debug << "log: " << it->logNum << ", puckstack stacksize: "
							<< puckstack.stacksize() << endl;
#endif
					logWriter.init(MSG_PUCKSTACK, puckstack);

					for (bool complete = false; !complete;)
						complete = logWriter.doWrite();
				} else if (type == MSG_TIMESTEPUPDATE) {
					timeframe.ParseFromArray(buffer, len);

					it->onTimeFrame = timeframe.timestep();

					//this should only evaluated to true only the first time that a message from a log is read
					if (nextTimeFrame > it->onTimeFrame)
						nextTimeFrame = it->onTimeFrame;

					break;
				}
			}
		}

		nextTimeFrame=getNextTimeFrame(inputLogsReader);
#ifdef DEBUG
		debug <<"timeframe: "<< nextTimeFrame << endl;
#endif
		timeframe.set_timestep(nextTimeFrame);
		logWriter.init(MSG_TIMESTEPUPDATE, timeframe);
		for (bool complete = false; !complete;)
			complete = logWriter.doWrite();
	}

	debug.close();
}

//get a list of files from a inputDir, open them and store them as a list of MessageReader vectors
void getInputLogs(string inputDir, vector<Log> &inputLogsReader,
		ofstream &debug) {
	DIR *dp;
	struct dirent *dirp;
	string fileName, tmp;
	int fd, logNum = 1;

	if ((dp = opendir(inputDir.c_str())) == NULL) {
#ifdef DEBUG
		debug << "Error(" << errno << ") opening " << inputDir << endl;
#endif
		exit(1);
	}

	while ((dirp = readdir(dp)) != NULL) {
		fileName = string(dirp->d_name);

		if (fileName.find(helper::defaultLogName) != string::npos) {
			tmp = inputDir + fileName;
			fd = open(tmp.c_str(), O_RDONLY);

			if (fd < 0) {
#ifdef DEBUG
				debug << "Could not open file name: " << fileName
						<< " for reading. Errno is " << errno << endl;
#endif
			} else {
#ifdef DEBUG
				debug << "File: " << fileName << " added for processing"
						<< endl;
#endif
				inputLogsReader.push_back(Log(fd, logNum));
				logNum++;
			}
		}
	}

	closedir(dp);
}

int main(int argc, char **argv) {
	string inputDir = "/tmp/input/", outputDir = "/tmp/output/";
	ofstream debug;
	debug.open("/tmp/logmerger_debug.txt", ios::out);

	if (argc > 1) {
		helper::Config config(argc, argv);
		inputDir = config.getArg("-in");
		outputDir = config.getArg("-out");

		if (inputDir.length() == 0)
			inputDir = "/tmp/input/";

		if (outputDir.length() == 0)
			outputDir = "/tmp/output/";
	}

	string mergedLog = helper::getNewName(outputDir + helper::defaultLogName);
	int mergedLogFd = open(mergedLog.c_str(), O_WRONLY | O_CREAT, 0644);
#ifdef DEBUG
	debug << "Reading logs from: " << inputDir << endl << "Writing logs to: "
			<< mergedLog << endl;
#endif
	if (mergedLogFd < 0) {
#ifdef DEBUG
		debug << "Could not open file name: " << mergedLog << " for writing"
				<< endl;
#endif
		exit(1);
	}

	vector<Log> inputLogsReader = vector<Log> ();
	MessageWriter logWriter(mergedLogFd);

	getInputLogs(inputDir, inputLogsReader, debug);

	sortMergeLog(inputLogsReader, logWriter, debug);

	return 0;
}
