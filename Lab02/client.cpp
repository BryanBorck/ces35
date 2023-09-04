#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cmath>
using namespace std::chrono_literals;

#define SERVER_PORT 8080
#define BUFSIZE 4096

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
    struct {
        float x_coordinate;
        float y_coordinate;
        float z_coordinate;
    } Drone_target;
};

int main(int argc, char **argv)
{
    int s, bytes;
    char buf[BUFSIZE]; 
    struct hostent *h;
    struct sockaddr_in channel;
    struct Drone drone = {1, {100, 200, 50}, {0, 0, 0}, {0, 0, 0}};

    if (argc != 2) {printf("Usage: client server-name"); exit(-1);}
    
    h = gethostbyname(argv[1]);
    if (!h) {printf("gethostbyname failed to locate %s", argv[1]); exit(-1);}
    
    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s <0) {printf("socket call failed"); exit(-1);}
    memset(&channel, 0, sizeof(channel));
    channel.sin_family= AF_INET;
    memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
    channel.sin_port= htons(SERVER_PORT);

    if (connect(s, (struct sockaddr *) &channel, sizeof(channel)) < 0) {
        printf("connect failed");
        exit(-1);
    }

    auto start_time = std::chrono::steady_clock::now();

    while (1) {

        auto current_time = std::chrono::steady_clock::now();

        std::chrono::duration<double> elapsed_seconds = current_time - start_time;

        drone.Drone_position.x_coordinate += drone.Drone_speed.x_coordinate * elapsed_seconds.count();
        drone.Drone_position.y_coordinate += drone.Drone_speed.y_coordinate * elapsed_seconds.count();
        drone.Drone_position.z_coordinate += drone.Drone_speed.z_coordinate * elapsed_seconds.count();

        bytes = recv(s, buf, BUFSIZE, 0);
        if (bytes < 0) {
            printf("recv call failed");
            exit(-1);
        }
        printf("Server Request: %s\n", buf);

        if (strcmp(buf, "request_id") == 0) {
            sprintf(buf, "%d", drone.Drone_ID);
            send(s, buf, strlen(buf) + 1, 0);
            bytes = recv(s, buf, BUFSIZE, 0);
            if (bytes < 0) {
                printf("recv call failed");
                exit(-1);
            }
            printf("Target: (%s)\n", buf);
            sscanf(buf, "%f, %f, %f", &drone.Drone_target.x_coordinate, &drone.Drone_target.y_coordinate, &drone.Drone_target.z_coordinate);

            drone.Drone_speed.x_coordinate = (drone.Drone_target.x_coordinate - drone.Drone_position.x_coordinate);
            drone.Drone_speed.y_coordinate = (drone.Drone_target.y_coordinate - drone.Drone_position.y_coordinate);
            drone.Drone_speed.z_coordinate = (drone.Drone_target.z_coordinate - drone.Drone_position.z_coordinate);

            float module = sqrt(pow(drone.Drone_speed.x_coordinate, 2) + pow(drone.Drone_speed.y_coordinate, 2) + pow(drone.Drone_speed.z_coordinate, 2));

            drone.Drone_speed.x_coordinate = 2 * drone.Drone_speed.x_coordinate / module;
            drone.Drone_speed.y_coordinate = 2 * drone.Drone_speed.y_coordinate / module;
            drone.Drone_speed.z_coordinate = 2 * drone.Drone_speed.z_coordinate / module;
        }
        
        if (strcmp(buf, "send_info") == 0) {
            sprintf(buf, "%d: (%.0f, %.0f, %.0f), (%.1f, %.1f, %.1f)", drone.Drone_ID, 
            drone.Drone_position.x_coordinate, drone.Drone_position.y_coordinate, drone.Drone_position.z_coordinate,
            drone.Drone_speed.x_coordinate, drone.Drone_speed.y_coordinate, drone.Drone_speed.z_coordinate);
            send(s, buf, strlen(buf) + 1, 0);
        }
        
        if (strstr(buf, "send_move")) {
            bytes = recv(s, buf, BUFSIZE, 0);
            if (bytes < 0) {
                printf("recv call failed");
                exit(-1);
            }
            printf("NEW Target: (%s)\n", buf);
            sscanf(buf, "%f, %f, %f", &drone.Drone_target.x_coordinate, &drone.Drone_target.y_coordinate, &drone.Drone_target.z_coordinate);

            drone.Drone_speed.x_coordinate = (drone.Drone_target.x_coordinate - drone.Drone_position.x_coordinate);
            drone.Drone_speed.y_coordinate = (drone.Drone_target.y_coordinate - drone.Drone_position.y_coordinate);
            drone.Drone_speed.z_coordinate = (drone.Drone_target.z_coordinate - drone.Drone_position.z_coordinate);

            int module = sqrt(pow(drone.Drone_speed.x_coordinate, 2) + pow(drone.Drone_speed.y_coordinate, 2) + pow(drone.Drone_speed.z_coordinate, 2));

            drone.Drone_speed.x_coordinate = 2 * drone.Drone_speed.x_coordinate / module;
            drone.Drone_speed.y_coordinate = 2 * drone.Drone_speed.y_coordinate / module;
            drone.Drone_speed.z_coordinate = 2 * drone.Drone_speed.z_coordinate / module;

            sprintf(buf, "ACK");
            send(s, buf, strlen(buf) + 1, 0);
        }

        if (strstr(buf, "send_disconnect")) {
            sprintf(buf, "ACK");
            break;
        }

        start_time = current_time;
    }

    printf("Closing connection\n");

    close(s);
    return 0;
}