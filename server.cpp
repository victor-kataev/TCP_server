#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <memory.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <queue>
#include <sstream>

using namespace std;


#define SERVER_MOVE "102 MOVE\\a\\b"
#define SERVER_TURN_LEFT "103 TURN LEFT\\a\\b"
#define SERVER_TURN_RIGHT "104 TURN RIGHT\\a\\b"
#define SERVER_PICK_UP "105 GET MESSAGE\\a\\b"
#define SERVER_LOGOUT "106 LOGOUT\\a\\b"
#define SERVER_OK "200 OK\\a\\b"
#define SERVER_LOGIN_FAILED "300 LOGIN FAILED\\a\\b"
#define SERVER_SYNTAX_ERROR "301 SYNTAX ERROR\\a\\b"
#define SERVER_LOGIC_ERROR "302 LOGIC ERROR\\a\\b"

#define SERVER_MOVE_LEN 12
#define SERVER_TURN_LEFT_LEN 17
#define SERVER_TURN_RIGHT_LEN 18
#define SERVER_PICK_UP_LEN 19
#define SERVER_LOGOUT_LEN 15
#define SERVER_OK_LEN 10
#define SERVER_LOGIN_FAILED_LEN 20
#define SERVER_SYNTAX_ERROR_LEN 20
#define SERVER_LOGIC_ERROR_LEN 19

#define CLIENT_USERNAME_LEN 12
#define CLIENT_CONFIRMATION_LEN 7
#define CLIENT_OK_LEN 99999
#define CLIENT_RECHARGING_LEN 12
#define CLIENT_FULL_POWER_LEN 12
#define CLIENT_MESSAGE_LEN 100

queue<string> g_commands;

class ComunicationException{};
class SyntaxError{};

class CRobot
{
public:

private:
    int m_x;
    int m_y;
};

class CMessenger
{
protected:
    char m_buffer[1000];
    const int BUFFSIZE = 1000;

    void sendMessage(int c_sockfd, const char *message, int mlen) const;
    int receiveMessage(int c_sockfd, size_t expectedLen);
};

void CMessenger::sendMessage(int c_sockfd, const char *message, int mlen) const
{
    if (send(c_sockfd, message, mlen, 0) == -1) {
        perror("write error");
        throw ComunicationException();
    }
}

int CMessenger::receiveMessage(int c_sockfd, size_t expectedLen)
{
    int mlen = 0;
    string tmpContainer;
    size_t foundPos;
    bool foundBool = false;

    if ((mlen = recv(c_sockfd, m_buffer, BUFFSIZE, 0)) == -1)
    {
        perror("read error");
        throw ComunicationException();
    }

    tmpContainer.append(m_buffer, mlen);

    while(true) {
        cout << m_buffer << endl;
        foundPos = tmpContainer.find("\\a\\b");

        if (foundPos != string::npos) {
            foundBool = true;
            string tmpCommand(tmpContainer, 0, foundPos);
            g_commands.push(tmpCommand);
            tmpContainer.erase(0, foundPos + 4);
            if (!tmpContainer.empty()) {
                continue;
            }
            break;
        } else {

            if (!foundBool && tmpContainer.size() > expectedLen)
                throw SyntaxError();

            if (send(c_sockfd, m_buffer, mlen, 0) == -1) {
                perror("write error");
                throw ComunicationException();
            }

            if ((mlen = recv(c_sockfd, m_buffer, BUFFSIZE, 0)) == -1) {
                perror("read error");
                throw ComunicationException();
            }

            if (mlen == 0)
                throw ComunicationException();

            tmpContainer.append(m_buffer, mlen);
        }
    }
    return (int)g_commands.front().length();
}

class CServer : public CMessenger
{
public:
    CServer()
    {
        setUpSocket();
        run();
    }

private:
    const int ECHOPORT = 5599;
    const unsigned short CLIENT_KEY = 45328;
    const unsigned short SERVER_KEY = 54621;
    int m_my_sockfd;
    sockaddr_in m_my_addr;
    sockaddr_in m_rem_addr;

    class CRobot {
    private:
        int m_sockfd;
        int m_x;
        int m_y;
    public:
        CRobot(int c_sockfd);
        void move();
        void extractCoords(string str);
    };


    void setUpSocket();
    void run();
    void clientRoutine(int c_sockfd);
    bool authenticate(int c_sockfd);
    unsigned short makeHash();
    int putCodeIntoBuffer(unsigned short code);
};

void CServer::setUpSocket()
{
    m_my_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(m_my_sockfd == -1) {
        cout << "Could not created socket\n";
        exit(1);
    }

    bzero(&m_my_addr, sizeof(m_my_addr));
    m_my_addr.sin_family = AF_INET;
    m_my_addr.sin_port = htons(ECHOPORT);

    if(bind(m_my_sockfd, (struct sockaddr *)&m_my_addr, sizeof(m_my_addr)) == -1) {
        cout << "Bind error\n";
        close(m_my_sockfd);
        exit(1);
    }
}

void CServer::run()
{
    if(listen(m_my_sockfd, 10) == -1) {
        cout << "Listen error\n";
        close(m_my_sockfd);
        exit(1);
    }

    cout << "Welcome to my freacking server!\n";

    vector<thread> threads;
    socklen_t rem_addr_len;
    int c_sockfd;

    while(1) {
        rem_addr_len = sizeof(m_rem_addr);
        c_sockfd = accept(m_my_sockfd, (struct sockaddr *)&m_rem_addr, &rem_addr_len);
        threads.emplace_back( thread(&CServer::clientRoutine, this, c_sockfd) );
        cout << "accepted new client\n";
    }

    close(m_my_sockfd);
}

void CServer::clientRoutine(int c_sockfd)
{
    int mlen;

    try {
        if(!authenticate(c_sockfd)) {
            cout << "Authentication failed\n";

            sendMessage(c_sockfd, SERVER_LOGIN_FAILED, SERVER_LOGIN_FAILED_LEN);
            close(c_sockfd);
            return;
        } else
            sendMessage(c_sockfd, SERVER_OK, SERVER_OK_LEN);
    }
    catch (SyntaxError e) {
        cout << "Authentication failed\n";

        sendMessage(c_sockfd, SERVER_LOGIN_FAILED, SERVER_LOGIN_FAILED_LEN);
        close(c_sockfd);
        return;
    }


    //robot creation here

    cout << "Authentication was successful\n";

    CRobot robot(c_sockfd);

    robot.move();
    robot.move();

    /*while (1) {

        try {
            mlen = receiveMessage(c_sockfd, CLIENT_USERNAME_LEN);
        }
        catch (ComunicationException e) {
            break;
        }


        if(mlen == 0)
            break;

        printf("Client #%d: %s\n", c_sockfd, m_buffer);

        try {
            sendMessage(c_sockfd, m_buffer, mlen);
        }
        catch (ComunicationException e) {
            break;
        }

        printf("sending %i bytes back to the client\n", mlen);
    }*/
    close(c_sockfd);
}

bool CServer::authenticate(int c_sockfd)
{
    int messLen;

    cout << "Waiting for username...\n";
    //getting username
    try {
        messLen = receiveMessage(c_sockfd, CLIENT_USERNAME_LEN);
    }
    catch (ComunicationException e) {
        return false;
    }
    catch(SyntaxError e) {
        throw SyntaxError();
    }

    unsigned short hash = makeHash();
    unsigned short confirmationCode = (hash + SERVER_KEY) % 65536;

    int codeLen = putCodeIntoBuffer(confirmationCode);

    //sending code to the client
    try {
        sendMessage(c_sockfd, m_buffer, codeLen);
    }
    catch (ComunicationException e) {
        return false;
    }

    unsigned short expected = (hash + CLIENT_KEY) % 65536;
    cout << "expecting code: " << expected << endl;

    if(g_commands.empty()) {
        //getting client's code
        try {
            messLen = receiveMessage(c_sockfd, CLIENT_CONFIRMATION_LEN);
        }
        catch (ComunicationException e) {
            return false;
        }
    }

    string response(g_commands.front());
    g_commands.pop();

    unsigned short clientConfirmationCode = (unsigned short)stoi(response);
    cout << "client confirmation code: " << clientConfirmationCode << endl;
//         unsigned short int expected = (hash + CLIENT_KEY) % 65536;

    return expected == clientConfirmationCode;
}

unsigned short CServer::makeHash()
{
    unsigned short hash = 0;
    string username(g_commands.front());
    g_commands.pop();
    for(int i = 0; i < username.length(); i++) {
        hash += username[i];
    }
    hash = (hash * 1000) % 65536;
    return hash;
}

int CServer::putCodeIntoBuffer(unsigned short code)
{

    string tmpStr = to_string(code);
    for (int i = 0; i < tmpStr.length(); ++i) {
        m_buffer[i] = tmpStr[i];
    }

    return (int)tmpStr.length();
}


CServer::CRobot::CRobot(int c_sockfd)
    :m_sockfd(c_sockfd), m_x(0), m_y(0)
{
}

void CServer::CRobot::move()
{
    if(g_commands.empty()) {
        sendMessage(m_sockfd, SERVER_MOVE, SERVER_MOVE_LEN);
        receiveMessage(m_sockfd, CLIENT_OK_LEN);
    }
    extractCoords(g_commands.front());
}

void CServer::CRobot::extractCoords(string str)
{
    stringstream ss;
    vector<int> tmpVec;

    /* Storing the whole string into string stream */
    ss << str;

    /* Running loop till the end of the stream */
    string temp;
    int found;
    while (!ss.eof()) {

        /* extracting word by word from stream */
        ss >> temp;

        /* Checking the given word is integer or not */
        if (stringstream(temp) >> found) {
            tmpVec.push_back(found);
        }
    }
    m_x = tmpVec[0];
    m_y = tmpVec[1];
}





int main()
{


    CServer server;

    return 0;
}
