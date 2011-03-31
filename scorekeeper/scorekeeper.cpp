#include <iostream>
#include <vector>
#include <string>
#include <dirent.h>
#include <fstream>
#include <fcntl.h>
#include <errno.h>
#include <algorithm>

#include "../common/globalconstants.h"
#include "../common/helper.h"
#include "../common/messagereader.h"
#include "../common/types.h"
#include "../common/regionrender.pb.h"

struct Home {
	int team, score;

	Home(int _team, int _score) :
		team(_team), score(_score) {
	}


};

using namespace std;

#ifdef DEBUG
ofstream debug;
#endif

bool sortAscending (Home i, Home j) { return (i.score>j.score); }


//read from input logs and write the final score file
void writeScore(vector<MessageReader> &inputLogsReader, ofstream &finalScore) {
	MessageType type;
	const void *buffer;
	int len;
	HomeScore homescore;
	vector<Home> home = vector<Home> ();

	//score all the team ids and their respective scores in a vector
	for (unsigned i = 0; i < inputLogsReader.size(); i++) {
		//read from a single log until we get an exception because we read everything
		while (true) {
			try {
				for (bool complete = false; !complete;)
					complete = inputLogsReader.at(i).doRead(&type, &len, &buffer);
			} catch (exception e) {
				break;
			}

			homescore.ParseFromArray(buffer, len);
			home.push_back(Home(homescore.team(), homescore.score()));
		}
	}

	//sort scores in ascending order
	sort(home.begin(), home.end(), sortAscending);

	//log and print the results
	for (unsigned i = 0; i < home.size(); i++)
	{
		finalScore << helper::toString(i + 1) + ".) Team: " << helper::toString(home.at(i).team) + ", Score: "
				+ helper::toString(home.at(i).score) << endl;
		cout << helper::toString(i + 1) + ".) Team: " << helper::toString(home.at(i).team) + ", Score: "
						+ helper::toString(home.at(i).score) << endl;
	}
}

//get a list of files from a inputDir, open them and store them as a list of MessageReader vectors
void getInputLogs(string inputDir, vector<MessageReader> &inputLogsReader) {
	DIR *dp;
	struct dirent *dirp;
	string fileName, tmp;
	int fd;

	if ((dp = opendir(inputDir.c_str())) == NULL) {
#ifdef DEBUG
		debug << "Error(" << errno << ") opening " << inputDir << endl;
#endif
		exit(1);
	}

	while ((dirp = readdir(dp)) != NULL) {
		fileName = string(dirp->d_name);

		if (fileName.find(helper::scoreKeeperLog) != string::npos) {
			tmp = inputDir + fileName;
			fd = open(tmp.c_str(), O_RDONLY);

			if (fd < 0) {
#ifdef DEBUG
				debug << "Could not open file name: " << fileName << " for reading. Errno is " << errno << endl;
#endif
			} else {
#ifdef DEBUG
				debug << "File: " << fileName << " added for processing" << endl;
#endif
				inputLogsReader.push_back(MessageReader(fd));
			}
		}
	}

	closedir(dp);
}

int main(int argc, char **argv) {
	string inputDir = helper::logDirectory, outputDir = argv[0];
	outputDir = outputDir.substr(0, outputDir.find_last_of("//") + 1);
	ofstream finalScore;

	string scoreLog = outputDir + helper::scoreKeeperFinalScore;
	finalScore.open(scoreLog.c_str(), ios::out);
#ifdef DEBUG
	debug.open("/tmp/logmerger_debug.txt", ios::out);
	debug << "Reading logs from: " << inputDir << endl << "Writing logs to: " << scoreLog << endl;
#endif

	vector<MessageReader> inputLogsReader = vector<MessageReader> ();

	getInputLogs(inputDir, inputLogsReader);
	writeScore(inputLogsReader, finalScore);

	finalScore.close();
#ifdef DEBUG
	debug.close();
#endif

	return 0;
}
