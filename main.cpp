#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define NORMAL_MODE 0
#define INPUT_MODE 1
#define OUTPUT_MODE 2
#define PIPE_MODE 3

#define IN 1
#define OUT 0

using namespace std;

// Xóa khoảng trắng dư thừa và các kí tự lỗi
// Ví dụ:"  ls    -a   -l  " --> "ls -a -l"
string formatString(string input)
{
	if (input.length() == 0)
		return input;
	for (int i = 0; i < input.length(); i++)
	{
		if (input[i] < 32 || input[i] > 126)
			input.replace(i, 1, " ");
	}
	while (input[0] == ' ')
		input.erase(0, 1);
	while (input[input.length() - 1] == ' ')
		input.erase(input.length() - 1, 1);
	while (input.find("  ") != -1)
		input.erase(input.find("  "), 1);
	return input;
}

// Tách special command thành 2 string phân cách bởi delim
// Ví dụ: "ls -al > out.txt" --> "ls -al" và "out.txt"
vector<string> splitSpecialCommand(string input, char delim)
{
	int posDelim = input.find(delim);
	if (input[posDelim + 1] == ' ')
		input.erase(posDelim + 1, 1);
	if (input[posDelim - 1] == ' ')
		input.erase(posDelim - 1, 1);
	posDelim = input.find(delim);
	string frontDelim = input.substr(0, posDelim);
	string backDelim = input.substr(posDelim + 1);
	vector<string> result;
	result.push_back(frontDelim);
	result.push_back(backDelim);
	return result;
}

// Tách token theo delim
vector<string> split(string str, char delim)
{
	vector<string> result;
	istringstream ss(str);
	string token;
	while (getline(ss, token, delim))
		result.push_back(token);
	return result;
}

// Chuyển vector sang char**
char **vectorToCharArray(vector<string> input)
{
	char **result = new char *[input.size() + 1];
	result[input.size()] = nullptr;
	for (int i = 0; i < input.size(); i++)
		result[i] = strdup(input[i].c_str());
	return result;
}

// Xử lí
void handle(string &input)
{
	int mode;
	char delim;
	bool onWait = true;
	if (input[input.length() - 1] == '&')
	{
		// Nếu có & thì xóa và bật cờ
		onWait = false;
		input.erase(input.length() - 1, 1);
	}
	if (input.find('<') != -1)
	{
		mode = INPUT_MODE;
		delim = '<';
	}
	else if (input.find('>') != -1)
	{
		mode = OUTPUT_MODE;
		delim = '>';
	}
	else if (input.find('|') != -1)
	{
		mode = PIPE_MODE;
		delim = '|';
	}
	else
	{
		mode = NORMAL_MODE;
		delim = ' ';
	}

	if (mode == 0)
	{
		vector<string> vectorString = split(input, delim);
		char **argc = vectorToCharArray(vectorString);
		pid_t pid = fork();
		if (pid < 0)
		{
			cout << "Fork failed!" << endl;
			exit(1);
		}
		if (pid == 0)
		{
			if (execvp(argc[0], argc) == -1)
			{
				cout << argc[0] << ": command not found" << endl;
				exit(1);
			}
		}
		else if (onWait)
			waitpid(pid, NULL, 0);
	}
	else
	{
		pid_t pid = fork();
		if (pid < 0)
		{
			cout << "Fork failed!" << endl;
			exit(1);
		}
		if (pid == 0)
		{
			vector<string> arrayOfSpecialCommand = splitSpecialCommand(input, delim);
			switch (mode)
			{
			case INPUT_MODE:
			case OUTPUT_MODE:
			{
				vector<string> vectorArgc = split(arrayOfSpecialCommand[0], ' ');
				char **argc = vectorToCharArray(vectorArgc);
				char *path = strdup(arrayOfSpecialCommand[1].c_str());
				int fd;
				if (mode == INPUT_MODE)
				{
					fd = open(path, O_RDONLY,0666);
					if (fd < 0)
					{
						cout << "Can not open file" << endl;
						exit(1);
					}
					dup2(fd, STDIN_FILENO);
				}
				else
				{
					fd = open(path, O_CREAT | O_WRONLY,0666);
					if (fd < 0)
					{
						cout << "Can not open file" << endl;
						exit(1);
					}
					dup2(fd, STDOUT_FILENO);
				}
				close(fd);
				if (execvp(argc[0], argc) == -1)
				{
					cout << argc[0] << ": command not found" << endl;
					exit(1);
				}
				break;
			}

			case PIPE_MODE:
			{
				vector<string> vectorArgcFront = split(arrayOfSpecialCommand[0], ' ');
				char **argcFront = vectorToCharArray(vectorArgcFront);
				vector<string> vectorArgcBack = split(arrayOfSpecialCommand[1], ' ');
				char **argcBack = vectorToCharArray(vectorArgcBack);

				pid_t pid1, pid2;
				int fd[2];
				pipe(fd);

				pid1 = fork();
				if (pid1 < 0)
				{
					cout << "Fork failed!" << endl;
					exit(1);
				}
				else if (pid1 == 0)
				{
					dup2(fd[IN], STDOUT_FILENO);
					close(fd[OUT]);
					close(fd[IN]);
					if (execvp(argcFront[0], argcFront) < 0)
					{
						cout << argcFront[0] << ": command not found" << endl;
						exit(1);
					}
				}
				else
				{
					pid2 = fork();
					if (pid2 < 0)
					{
						printf("fork failed!\n");
						exit(1);
					}
					else if (pid2 == 0)
					{
						dup2(fd[OUT], STDIN_FILENO);
						close(fd[IN]);
						close(fd[OUT]);
						if (execvp(argcBack[0], argcBack) < 0)
						{
							cout << argcBack[0] << ": command not found" << endl;
							exit(1);
						}
					}
					else
					{
						close(fd[IN]);
						close(fd[OUT]);
						waitpid(-1, NULL, 0);
						waitpid(-1, NULL, 0);
					}
				}
				break;
			}
			}
		}
		else if (onWait)
			waitpid(pid, NULL, 0);
	}
}

// Vòng lặp
void excute(bool isRunning)
{
	string backupData = "";
	while (isRunning)
	{
		string input;
		cout << endl
			 << "osh> ";
		fflush(stdout);
		getline(cin, input);
		input = formatString(input);
		// Người dùng không nhập gì
		if (input.length() == 0)
			continue;

		// Nhập vào exit thì thoát
		if (input.compare("exit") == 0)
			exit(0);

		// Nhập vào !! thì xuất câu lệnh trước đó
		if (input.compare("!!") == 0)
		{
			if (backupData.length() != 0)
				input = backupData;
			else
			{
				// Nếu không có lệnh trước đó
				cout << "No commands in history" << endl;
				continue;
			}
		}
		else
			// Cập nhật vào history
			backupData = input;

		//Xử lí
		handle(input);
	}
}

int main(void)
{
	// Lặp đến khi hoàn thành
	excute(true);
	return 0;
}