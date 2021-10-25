#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <istream>
#include <fstream>
#include <vector>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>

void errorOut();
int runBuiltInCmds(const std::vector<std::string> parsed, std::vector<std::string> *path);
std::vector<std::string> parseInput(const std::string input, const char parseAt);
void takeInput(std::vector<std::string> *path);
void runCommand(std::string input, std::vector<std::string> *path);
void runChild(std::string parsedCommand, std::vector<std::string> *path);
void runExec(const std::vector<std::string> args, std::vector<std::string> *path, const std::string location = "\0");

/* 
    Runs the shell based on the args given if exactly 1 arg is given if runs in interactive, 
    if exactly 2 are given it runs in batch, and in any other case it errors.
    In batch mode it will continuously read lines from the file and run the line in runCommand.
*/
int main(int argc, char *argv[])
{

    std::vector<std::string> *path = new std::vector<std::string>();
    path->push_back("/bin");

    if (argc == 1)
    {
        while (true)
        {

            takeInput(path);
        }
    }
    else if (argc == 2)
    {

        std::string input;
        std::ifstream file;

        file.open(argv[1]);

        if (!file)
        {

            errorOut();
            exit(1);
        }

        while (std::getline(file, input))
        {

            runCommand(input, path);
        }

        file.close();
        exit(0);
    }
    else
    {

        errorOut();
        exit(1);
    }
}

/*
    takeInput outputs the prompt and takes in the user's input and runs the line in runCommand.
*/
void takeInput(std::vector<std::string> *path)
{

    std::string input;

    std::cout << "wish> ";
    std::getline(std::cin, input);

    runCommand(input, path);
}
/*
    runCommand takes the user's input and parses it for parallel commands, 
    then it will run those commands in a for loop checking if they are built-in with runBuiltInCmds.
    Otherwise, they will fork into a child that will run runChild with their assigned parsed command.
    The parent will wait until all children are done before it proceeds.
*/
void runCommand(std::string input, std::vector<std::string> *path)
{

    std::vector<std::string> parsedCommands = parseInput(input, '&');

    int children = parsedCommands.size();
    pid_t pids[children];

    for (int i = 0; i < (int)parsedCommands.size(); i++)
    {

        std::vector<std::string> parsedArgs = parseInput(parsedCommands[i], ' ');

        if (!parsedArgs.empty())
        {

            if (runBuiltInCmds(parsedArgs, path) == 0)
            {

                if ((pids[i] = fork()) < 0)
                {

                    errorOut();
                }
                else if (pids[i] == 0)
                {

                    runChild(parsedCommands[i], path);
                    exit(0);
                }
            }
        }
        else
        {

            continue;
        }
    }
    int status;
    while (children > 0)
    {
        wait(&status);
        --children;
    }
}

/*
    runChild parses the given string by checking for the redirect character and parsing it at that point.
    It then parses the string at the whitespaces to find args. 
    Then it passes the parsedArgs and, depending on if a redirect exists, the redirect location to runExec.
*/
void runChild(std::string parsedCommand, std::vector<std::string> *path)
{

    if (parsedCommand.find('>') != std::string::npos)
    {

        std::vector<std::string> parsedRedirect = parseInput(parsedCommand, '>');
        std::vector<std::string> parsedArgs = parseInput(parsedRedirect[0], ' ');

        if (parsedRedirect.size() == 2)
        {

            std::vector<std::string> redirectCheck = parseInput(parsedRedirect[1], ' ');

            if (redirectCheck.size() == 1)
            {

                runExec(parsedArgs, path, redirectCheck[0]);
            }
            else
            {

                errorOut();
            }
        }
        else
        {

            errorOut();
        }
    }
    else
    {

        std::vector<std::string> parsedArgs = parseInput(parsedCommand, ' ');
        runExec(parsedArgs, path);
    }
}

/*
    runExec first checks if the executables are valid inside the path and sets the validPathIndex.
    If there is no validPathIndex it errors and returns.
    If there is it will continue by creating an array for args and adding the values from
    the args vector into it as well as adding the null pointer and creating a new string called
    newPath that can be used as the first argument in execv.
    If a location for the redirect is given it will open the file at that location and redirect the stdout
    and stderr file descriptors to that file and run execv.
    Otherwise, it will run execv like normal.
*/
void runExec(const std::vector<std::string> args, std::vector<std::string> *path, const std::string location)
{

    int validPathIndex = -1;

    for (int i = 0; i < (int)path->size(); i++)
    {

        if (access((path->at(i) + '/' + args[0]).c_str(), X_OK) == 0)
        {

            validPathIndex = i;
        }
    }

    if (validPathIndex < 0)
    {

        errorOut();
        return;
    }

    char *myargs[args.size() + 1];

    for (int i = 0; i < (int)args.size(); i++)
    {

        myargs[i] = strdup(args[i].c_str());
    }

    myargs[args.size()] = NULL;

    std::string newPath = path->at(validPathIndex) + '/' + myargs[0];

    if (location.compare("\0") != 0)
    {

        int fd = open(location.c_str(), O_RDWR | O_CREAT | O_TRUNC);

        dup2(fd, 1);
        dup2(fd, 2);

        close(fd);

        execv(newPath.c_str(), myargs);
    }
    else
    {
        execv(newPath.c_str(), myargs);
    }
}

/*
    runBuiltInCmds takes the parsed arguments and cross references the first argument in parsed
    with the list of built in commands. It then sets the value of currentArg and enters a switch
    statement where the proper command is ran and it returns 1 or no command is ran and it returns 0 which tells
    the program running it that parsed does not contain a built in command.
*/
int runBuiltInCmds(const std::vector<std::string> parsed, std::vector<std::string> *path)
{

    std::string builtIn[3] = {"cd", "path", "exit"};
    int currentArg = 0;

    for (int i = 0; i < 3; i++)
    {

        if (parsed[0].compare(builtIn[i]) == 0)
        {

            currentArg = i + 1;
        }
    }

    switch (currentArg)
    {
    case 1:

        if (parsed.size() != 2)
        {

            errorOut();
            return 1;
        }
        else
        {

            if (chdir(strdup(parsed[1].c_str())) != 0)
            {

                errorOut();
                return 1;
            }
            else
            {

                return 1;
            }
        }

    case 2:
        if (parsed.size() == 1)
        {

            path->clear();
            return 1;
        }
        else
        {

            std::vector<std::string> *newPath = new std::vector<std::string>();

            for (int i = 0; i < (int)parsed.size() - 1; i++)
            {

                newPath->push_back(parsed[i + 1]);
            }

            *path = *newPath;
            delete newPath;
            return 1;
        }

    case 3:

        if (parsed.size() != 1)
        {

            errorOut();
            return 1;
        }
        else
        {

            delete path;
            exit(0);
        }
    }

    return 0;
}

/*
    errorOut is a utility for easily outputting the correct error message
*/
void errorOut()
{

    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

/*
    parseInput takes an input string and parses it based on the character given by
    changing the string to a stringstream and using getline with a custom delimiter
    and adding each phrase to a vector and returning it.
*/
std::vector<std::string> parseInput(const std::string input, const char parseAt)
{

    std::vector<std::string> inputWords;
    std::stringstream inputString(input);
    std::string word;

    while (std::getline(inputString, word, parseAt))
    {

        if (!word.empty())
        {

            inputWords.push_back(word);
        }
    }

    return inputWords;
}
