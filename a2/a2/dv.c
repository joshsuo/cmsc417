#include <assert.h>
#include <stdio.h>
#include <poll.h>
#include <netdb.h>
#include <time.h>

#include "dv.h"
#include "es.h"
#include "ls.h"
#include "rt.h"
#include "n2h.h"

#define MAX_NODES 256
#define MAX_BUFFER_LEN 1024

// global variables
// you may want to take a look at each header files (es.h, ls.h, rt.h)
// to learn about what information and helper functions are already available to you
extern struct el *g_lst;  // 2-D linked list of parsed event config file
extern struct link *g_ls; // current host's link state storage
extern struct rte *g_rt;  // current host's routing table

// this function is our "entrypoint" to processing "list of [event set]s"
void walk_event_set_list(int pupdate_interval, int evset_interval, int verbose)
{
	struct el *es; // event set, as an element of the global list

	// TODO: UNUSED() is here to suppress unused variable warnings,
	// remove this and use this variable!
	UNUSED(verbose);

	assert(g_lst->next);

	print_el();

	if(evset_interval < 1) {
		evset_interval = 1;
	}

	if(pupdate_interval < 1) {
		pupdate_interval = 1;
	}

	// for each [event set] in global parsed 2-d list
	for (es = g_lst->next; es != g_lst; es = es->next)
	{
		process_event_set(es);
		//printf("[es] >>>>>>> process event done <<<<<<<<<<<\n");

		// HINT: this function should block for evset_interval seconds
		dv_process_updates(pupdate_interval, evset_interval);

		//printf("[es] >>>>>>> dv process update done <<<<<<<<<<<\n");

		printf("[es] >>>>>>> Start dumping data stuctures <<<<<<<<<<<\n");
		print_n2h();
		print_ls();
		print_rt();
	}

	// now all event sets have been processed
	// continue running, so "remaining" updates on the network can be processed (i.e. count to infinity)
	// TODO: uncomment line below, and modify dv_process_updates() to loop forever when evset_interval is 0
	// NOTE: we advise you to make this modification last, after you have everything else working 
	dv_process_updates(pupdate_interval, 0);
}

// iterate through individual "event" in single [event set]
// and dispatch each event using `dispatch_single_event()`
// you wouldn't need to modify this function, but make sure you understand it!
void process_event_set(struct el *es)
{
	struct es *ev_set; // event set, as a list of single events
	struct es *ev;	   // single event

	assert(es);

	// get actual event set's head from the element `struct el`
	ev_set = es->es_head;
	assert(ev_set);

	printf("[es] >>>>>>>>>> Dispatch next event set <<<<<<<<<<<<<\n");
	// for each "event"s in [event set]
	for (ev = ev_set->next; ev != ev_set; ev = ev->next)
	{
		printf("[es] Dispatching next event ... \n");
		dispatch_single_event(ev);
	}
}

int set_packet(uint8_t *buffer) {
	struct rte *i;
	uint16_t counter = 0;
	uint8_t type = 0x7;
	uint8_t version = 0x1;
	memcpy(buffer, &type, 1);
	memcpy(buffer +1, &version, 1);

	for(i = g_rt->next; i != g_rt; i = i->next) {
		uint16_t dest = htons(i->d);
		memcpy(buffer + (counter +1) *4, &dest, 2);
		uint16_t cost_value = htons(i->c);
		memcpy(buffer + (counter +1) *4 +2, &cost_value, 2);
		counter++;
	}

	uint16_t counter_n = htons(counter);
	memcpy(buffer+2, &counter_n, 2);

	return counter;
}

void send_to_neighbor(struct link *l) {
	node curr_host = get_myid();
	node node_to_send;
	int port_to_send = l->peer_port;

	struct sockaddr_in server_addr;

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr = getaddrbyhost(gethostbynode(l->peer));
	server_addr.sin_port = htons(port_to_send);

	uint8_t buffer[1024] = {0};
	int entry_count = set_packet(buffer);
	int size = (entry_count + 1) * 4;

	// for(int n = 0; n<size; n++)
	// 	printf("%02x", buffer[n]);	
	//printf("\n");

	sendto(l->sockfd, buffer, size, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));

}

void send_all_neighbors() { //) {
	//node curr_host = get_myid();
	// if(curr_host != ev->peer0 && curr_host != ev->peer1) {
	// 	return;
	// }

	for(struct link *l = g_ls->next; l != g_ls; l = l->next) {
		assert(l);

		send_to_neighbor(l);
	}

}

void send_all_neighbors_except(node n) { //) {
	//node curr_host = get_myid();
	// if(curr_host != ev->peer0 && curr_host != ev->peer1) {
	// 	return;
	// }

	for(struct link *l = g_ls->next; l != g_ls; l = l->next) {
		assert(l);

		//send_to_neighbor(l);
		if(l->peer != n) send_to_neighbor(l);
	}

}

struct link *find_link_by_peer(node peer) {
	struct link *l;
	for(l = g_ls->next; l != g_ls; l = l->next) {
		if(l->peer == peer) {
			return l;
		}
	}
	return 0x0;
}

struct rte *find_rte_by_nh(node n) {
	struct rte *i;

	for (i = g_rt->next; i != g_rt; i = i->next)
	{
		if (i->nh == n)
			return i;
	}
	return 0x0;

}

// dispatch a event, update data structures, and
// TODO: send link updates to current host's direct neighbors
void dispatch_single_event(struct es *ev)
{
	assert(ev);
	struct link *l;

	// for each event type (establish / update / teardown), you should:
	// detect if this event is relevant to current host (check doc comments of each functions that update the data structures)
	// if yes, propagate updates to your direct neighbors
	// you might want to add your own helper that handles sending to neighbors

	print_event(ev);

	switch (ev->ev_ty)
	{
	case _es_link:
		//print_event(ev);

		add_link_if_local(ev->peer0, ev->port0, ev->peer1, ev->port1, ev->cost, ev->name);
		if(ev->peer1 == get_myid()) {
			update_rte(ev->peer0, ev->cost, ev->peer0);
			print_rte(find_rte(ev->peer0));
		} else {
			update_rte(ev->peer1, ev->cost, ev->peer1);
			print_rte(find_rte(ev->peer1));
		}
		
		//l = find_link(ev->name);
		send_all_neighbors();
		
		break;
	case _ud_link:
		//print_event(ev);

		if(find_link(ev->name) == NULL || ev->cost < 0) break;

		struct es *up_es = geteventbylink(ev->name);

		if(up_es->peer1 == get_myid()) {
			update_rte(up_es->peer0, ev->cost, up_es->peer0);
			print_rte(find_rte(ev->peer0));	
		} else {
			update_rte(up_es->peer1, ev->cost, up_es->peer1);
			print_rte(find_rte(ev->peer1));
		}

		//l = find_link(ev->name);
		send_all_neighbors();

		ud_link(ev->name, ev->cost);

		break;
	case _td_link:
		//print_event(ev);

		if(find_link(ev->name) == NULL) break;

		struct es *del_es = geteventbylink(ev->name);
		node peer;
		
		if(del_es->peer1 == get_myid()) {
			peer = del_es->peer0;
		} else {
			peer = del_es->peer1;
		}

		update_rte(peer, -1, peer);
		print_rte(find_rte(peer));

		for (struct rte *e = g_rt->next; e != g_rt; e = e->next)
		{
			if (e->nh == peer) {
				update_rte(e->d, -1, e->d);
				print_rte(e);
			}
		}

		//l = find_link(del_es->name);
		send_all_neighbors();

		del_link(del_es->name);

		break;
	default:
		printf("[es]\t\tUnknown event!\n");
		break;
	}

}

// this function should execute for `evset_interval` seconds
// it will recv updates from neighbors, update the routing table, and send updates back
// it should also handle sending periodic updates to neighbors
// TODO: implement this function; pseudocode has been provided below for your reference
void dv_process_updates(int pupdate_interval, int evset_interval)
{
	// UNUSED() are here to suppress unused variable warnings,
	// remove these and use these variables!
	//UNUSED(pupdate_interval);
	//UNUSED(evset_interval);

	// you may want to take a timestamp when function is first called (t0),
	// so you know when to break out of this function and continue to next event set

	// loop
	//     determine how long you should wait for (pupdate_interval for first iteration of loop)
	//     use `select()` to block for amount you determined above
	//     if `select()` returns because of timeout
	//         send periodic updates to neighbors using `send_periodic_updates()`
	//     if `select()` returns because of data available
	//         recv updates from neighbors (you may want to write your own recv helper function)
	//         update routing table
	//         send updates to neighbors
	//     determine how long you should wait for `select()` in next iteration of loop
	//     (HINT: which one comes early? next periodic update or next event set execution?)
	//     make sure that the whole function don't block for longer than `evset_interval` seconds!

	time_t t0 = time(NULL);

	int timeout_sec = pupdate_interval;

	node nodes[MAX_NODES];
	int socket_counter = 0;
	struct pollfd sockets[MAX_NODES] = {0};

	for(struct link *l = g_ls->next; l != g_ls; l = l->next) {
		if(l->sockfd != -1) {
			sockets[socket_counter].fd = l->sockfd;
			sockets[socket_counter].events = POLLIN;
			nodes[socket_counter] = l->peer;

			socket_counter++;
		}

	}

	int ready = 0;
	int curr_host = get_myid();
	while(1) {
		ready = poll(sockets, socket_counter, timeout_sec *1000);
		if(ready < 0) {
			fprintf(stderr, "poll() error\n");
			exit(1);
		} else if(ready == 0) {
			fprintf(stderr, "poll() timeout\n");
			send_periodic_updates();
		} else {
			for(int i = 0; i < socket_counter; i++) {
				if(sockets[i].revents && POLLIN) { // socket available
					//printf("[es] >>>>>>> if POLLIN <<<<<<<<<<<\n");
					struct sockaddr_storage client_addr;
					socklen_t client_addr_len = sizeof(client_addr);

					uint8_t buffer[MAX_BUFFER_LEN] = {0};
					int n = recvfrom(sockets[i].fd, buffer, MAX_BUFFER_LEN, MSG_WAITALL, (struct sockaddr *)&client_addr, &client_addr_len);

					uint16_t num_updates;
					memcpy(&num_updates, buffer+2, 2);
					num_updates = ntohs(num_updates);

					uint16_t dest = 0;
					uint16_t min_cost = 0;
					node neighbor_node = nodes[i];
					for(int u = 0; u<num_updates; u++) {
						memcpy(&dest, buffer +4 +4 *u, 2);
						memcpy(&min_cost, buffer +4 +4 *u +2, 2);

						dest = ntohs(dest);
						min_cost = ntohs(min_cost);

						if(get_myid() == dest) continue;

						
						if(curr_host != dest) {
							struct rte *d_v = find_rte(dest);
							struct rte *d_x = find_rte(neighbor_node);

							if((int16_t)min_cost == -1) {
								struct link *l = find_link_by_peer(dest);
								if(l == 0x0) {
									update_rte(dest, -1, dest);
								} else { 
									update_rte(dest, l->c, dest);
								}
								print_rte(find_rte(dest));
								//send_all_neighbors();
								send_all_neighbors_except(neighbor_node);
							} else {
								int cost_updated = 0;
								if(min_cost != (d_v->c - d_x->c) && d_v->c != -1 && d_v->nh == neighbor_node) {
									cost_updated = 1;
								}
								if((d_x->c + min_cost) < d_v->c || d_v->c == -1 || cost_updated == 1)  {
									update_rte(dest, d_x->c + min_cost, neighbor_node);
									print_rte(find_rte(dest));
									//send_all_neighbors();
									send_all_neighbors_except(neighbor_node);
								}
							}
						}
					}
					//printf("-----------------------------\n");
					//for(int bi = 0; bi<n; bi++) printf("%02x", buffer[bi]);
					//printf("\n");
				}
			}
			//print_rt();
		}

		time_t t1 = time(NULL);
		double elapsed_sec = difftime(t1, t0);
		if(elapsed_sec >= evset_interval) break;
		int remain = evset_interval - elapsed_sec;
		if(timeout_sec > remain) {
			timeout_sec = remain < 1 ? 1 : remain;
		}
		
	}
	//print_rt();

}

// read current host's routing table, and send updates to all neighbors
// TODO: implement this function
// HINT: if you implemented a helper that handles sending to neighbors in `dispatch_single_event()`,
// you can reuse that here!
void send_periodic_updates() {
	send_all_neighbors();
}