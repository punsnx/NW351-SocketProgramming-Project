#include "../header/client.h"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <server_ip> <port> <file1> [file2 ...]" << endl;
        return 1;
    }

    string serverIp = argv[1];
    int port = atoi(argv[2]);

    try {
        ReliableUDPClient client(serverIp, port);

        for (int i = 3; i < argc; ++i) {
            string fname = argv[i];
            MetaData resp{};

            bool sendRequestSucceed = false;
            while (!sendRequestSucceed) {
                if(!client.requestFile(fname, resp)) {
                    cerr << "Failed to get ACK from server for file: " << fname << "\n";
                    continue;
                }else {
                    sendRequestSucceed = true;
                    cerr << "Got ACK from server for file: " << fname << "\n";
                }
            }
            if (!resp.fileExists || resp.fileSize <= 0 || resp.totalSegments <= 0) {
                cout << "Server reports file not found: " << fname << "\n";
                continue;
            }
            client.receiveFile(resp.filename[0] ? string(resp.filename) : fname, resp.totalSegments);
        }
    } catch (const exception& ex) {
        cerr << "Fatal: " << ex.what() << "\n";
        return 2;
    }

    return 0;
}