#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
using namespace std::chrono_literals;

#define SERVER_PORT 8080
#define BUF_SIZE 4096
#define QUEUE_SIZE 10

struct Target {
    struct {
        float x_coordinate;
        float y_coordinate;
        float z_coordinate;
    } target;
};

struct Drone {
    int Drone_ID;
    struct {
        float x_coordinate;
        float y_coordinate;
        float z_coordinate;
    } Drone_position;
    struct {
        float x_coordinate;
        float y_coordinate;
        float z_coordinate;
    } Drone_speed;
};

int main(int argc, char *argv[]) {
    int s, b, l, sa, bytes, on = 1;
    char buf[BUF_SIZE];
    struct sockaddr_in channel;
    struct Target target = {250, 300, 50};
    struct Drone drone = {0, {0, 0, 0}, {0, 0, 0}};

    std::vector<std::string> authorized_ids = {"1", "2", "31"};

    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    channel.sin_addr.s_addr = htonl(INADDR_ANY);
    channel.sin_port = htons(SERVER_PORT);

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {printf("socket call failed"); exit(-1);}
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    b = bind(s, (struct sockaddr *) &channel, sizeof(channel));
    if (b < 0) {printf("bind failed"); exit(-1);}

    l = listen(s, QUEUE_SIZE);
    if (l < 0) {printf("listen failed"); exit(-1);}

    printf("Server is ready\n");

    while (true) {
        sa = accept(s, 0, 0);
        if (sa < 0) {
            printf("accept failed");
            exit(-1);
        }

        strcpy(buf, "request_id");
        send(sa, buf, strlen(buf) + 1, 0);
        bytes = recv(sa, buf, BUF_SIZE, 0);
        if (bytes <= 0) { close(sa); continue; }

        bool found = false;
        for (const std::string& str : authorized_ids) {
            if (strcmp(str.c_str(), buf) == 0) {
                found = true;
                break;
            }
        }
        if (bytes <= 0 || found == false) {
            printf("Unauthorized connection or connection error. Disconnecting client.\n");
            strcpy(buf, "send_disconnect");
            send(sa, buf, strlen(buf) + 1, 0);
            close(sa);
            continue;
        }
        printf("Authorized drone connected: ID = %s\n", buf);
        sprintf(buf, "%.0f, %.0f, %.0f", target.target.x_coordinate, target.target.y_coordinate, target.target.z_coordinate);
        send(sa, buf, strlen(buf) + 1, 0);

        auto last_info_request_time = std::chrono::steady_clock::now();
        auto last_sent_time = std::chrono::steady_clock::now();
        bool done = true;

        while (true) {
            auto current_time = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(current_time - last_info_request_time) >= 5s) {
                printf("Sending info instruction to drone\n");
                strcpy(buf, "send_info");
                send(sa, buf, strlen(buf) + 1, 0);

                bytes = recv(sa, buf, BUF_SIZE, 0);
                if (bytes <= 0) { close(sa); continue; }
                printf("Drone Response: %s\n", buf);

                sscanf(buf, "%d: (%f, %f, %f), (%f, %f, %f)", &drone.Drone_ID, 
                &drone.Drone_position.x_coordinate, &drone.Drone_position.y_coordinate, &drone.Drone_position.z_coordinate,
                &drone.Drone_speed.x_coordinate, &drone.Drone_speed.y_coordinate, &drone.Drone_speed.z_coordinate);

                last_info_request_time = current_time;
            }

            if (std::chrono::duration_cast<std::chrono::seconds>(current_time - last_sent_time) >= 10s && done) {
                printf("Sending target change instruction to drone\n");
                strcpy(buf, "send_move");
                send(sa, buf, strlen(buf) + 1, 0);

                target = {300, 100, 60};
                sprintf(buf, "%.0f, %.0f, %.0f", target.target.x_coordinate, target.target.y_coordinate, target.target.z_coordinate);
                send(sa, buf, strlen(buf) + 1, 0);

                bytes = recv(sa, buf, BUF_SIZE, 0);
                if (bytes <= 0) { close(sa); continue; }
                printf("Drone Response: %s\n", buf);

                last_sent_time = current_time;
                done = false;
            }

            if ((target.target.x_coordinate - 10) < drone.Drone_position.x_coordinate && drone.Drone_position.x_coordinate < (target.target.x_coordinate + 10) &&
                (target.target.y_coordinate - 10) < drone.Drone_position.y_coordinate && drone.Drone_position.y_coordinate < (target.target.y_coordinate + 10) &&
                (target.target.z_coordinate - 10) < drone.Drone_position.z_coordinate && drone.Drone_position.z_coordinate < (target.target.z_coordinate + 10))
            {
                printf("Target achieved.\n");
                strcpy(buf, "send_disconnect");
                send(sa, buf, strlen(buf) + 1, 0);
                close(sa);
                close(s);
                break;
            }
        }
    }

    close(s);
    return 0;
}
