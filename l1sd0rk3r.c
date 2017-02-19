/*
 * made by love by betmenwasdie
 * Compile:
 * Linux: gcc -g -Wall -pthread -std=c99 l1sd0rk3r.c -lpthread -o l1sd0rk3r
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef WINUSER
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define close closesocket
#define sleep Sleep
#else
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef unsigned long u_long;

#endif

#define MAXLIMIT 256
#define say printf
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0 
#endif

typedef struct {
	unsigned int number_of_threads;			/* Número de threads. */
	unsigned int scan_method;				/* Método de scaneamento. */
	char *top_level;				/* Top-level list. */
	char *dork_list;				/* Lista de dorks. */
	char *path_filter;				/* Path para buscar. */
	char *data_filter;				/* Dados para buscar no conteúdo da página. */
	char *search_engine;			/* Lista de buscadores utilizados. */
	char *results_domains;			/* Arquivo de resultados contendo apenas o domínio. */
	char *results_full_url;		/* Arquivo de resultados com URL completa. */
} instance_t;

typedef struct {
	unsigned int index;						/* Identificador. */
	char *line;					/* Linha pega da lista de dorks. */
} param_t;

typedef struct {
	unsigned int counter_a;					/* Controle das threads. */
	unsigned int counter_b ;
	unsigned int counter_c;
} thread_t;

typedef struct {
	unsigned int total_sites;				/* Total de hosts válidos encontrados. */
	unsigned int total_sites_all;			/* Total de hosts encontrados em todos os buscadores e com repetição. */
} statistics_t;

typedef struct {							/* Controle das requisições HTTP. */
	char *content;
	unsigned int length;
	unsigned int e_200_OK;
} http_request_t;

instance_t *instance;
thread_t *thread;
statistics_t *statistics;

static void *core (void *tparam);
static void bing (const char *dork);
static void google (const char *dork);
static void hotbot (const char *dork);
static void duckduckgo (const char *dork);
static void extract_urls_and_domains (char *content);
static void filter_url (const char *url, const unsigned int protocol);
static unsigned int path_and_data_filter (const char *domain, const unsigned int protocol);
static http_request_t *http_request_get (const char *domain, const char *path);
static unsigned int domain_exists (const char *domain);
static void save (const char *path, const char *content);
static unsigned int file_exists (const char *path);
void *xmalloc (const unsigned int size);
//void *xcalloc (const unsigned int size);
void *xcalloc (const unsigned int nmemb, const unsigned int size);
static void die (const char *content, const unsigned int exitCode);
static void banner (void);
static void help (void);

int main (int argc, char **argv) {
	
#ifdef WINUSER
	WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) 
      die("WSAStartup failed!", EXIT_FAILURE);
#endif
	
	if (!(instance = (instance_t *) xcalloc(1, sizeof(instance_t)))) {
		say("Error to alloc memory - instance_t struct.\n");
		exit(EXIT_FAILURE);
	}
	
	instance->scan_method = 3;

	if (!(statistics = (statistics_t *) xcalloc(1, sizeof(statistics_t)))) {
		say("Error to alloc memory - statistics_t struct.\n");
		exit(EXIT_FAILURE);
	}
	
#define COPY_ARGS_TO_STRUCT(PTR)\
	if (a+1 < argc)\
		if ((PTR = (char *) xcalloc((strlen(argv[a+1])+1), sizeof(char))) != NULL) {\
			memcpy(PTR, argv[a+1], strlen(argv[a+1])); }
	
	for (int a=0; a<argc; a++) {
		if (strcmp(argv[a], "-l") == 0) { COPY_ARGS_TO_STRUCT(instance->dork_list) }
		if (strcmp(argv[a], "-t") == 0) { if (a+1 < argc) instance->number_of_threads = atoi(argv[a+1]); }
		if (strcmp(argv[a], "-v") == 0) { COPY_ARGS_TO_STRUCT(instance->top_level) }
		if (strcmp(argv[a], "-p") == 0) { COPY_ARGS_TO_STRUCT(instance->path_filter) }
		if (strcmp(argv[a], "-d") == 0) { COPY_ARGS_TO_STRUCT(instance->data_filter) }
		if (strcmp(argv[a], "-c") == 0) { COPY_ARGS_TO_STRUCT(instance->search_engine) }
		if (strcmp(argv[a], "-m") == 0) { if (a+1 < argc) instance->scan_method = atoi(argv[a+1]); }
	}
	
	if (instance->scan_method != 3)
		if (!(instance->scan_method == 1 || instance->scan_method == 2)) {
			banner();
			help();
			say("\n\n    Scan methods found: 1 or 2.\n");
			exit(EXIT_FAILURE);
		}
	
	if (((argc-1) % 2) != 0) {
		banner();
		help();
	} else if (instance->number_of_threads == 0 && instance->dork_list == NULL && instance->top_level == NULL && 
		instance->path_filter == NULL && instance->data_filter == NULL &&  instance->search_engine == NULL &&
		instance->scan_method == 3) {
		banner();
		help();
	} else {
		if (instance->number_of_threads == 0)
			instance->number_of_threads = 1;
		
		if (instance->dork_list == NULL) {
			say("It did not specify the list of dorks.\nRun %s --help to show help menu.\n", argv[0]);
			exit(EXIT_FAILURE);
		}
		
		if (file_exists(instance->dork_list) == FALSE) {
			say("File not exists: %s.\n", instance->dork_list);
			exit(EXIT_FAILURE);
		}
		
		instance->results_domains = (char *) xcalloc(MAXLIMIT, sizeof(char));
		sprintf(instance->results_domains, "%s-domains.txt", instance->dork_list);
		
		instance->results_full_url = (char *) xcalloc(MAXLIMIT, sizeof(char));
		sprintf(instance->results_full_url, "%s-full_url.txt", instance->dork_list);
		
		if ((thread = (thread_t *) xmalloc(sizeof(thread_t))) != NULL) {
			thread->counter_a = instance->number_of_threads;
			thread->counter_b = 1;
			thread->counter_c = 0;
		} else {
			say("Error to alloc memory - thread_t struct.\n");
			exit(EXIT_FAILURE);
		}
		
		if (thread->counter_a == 0)
			thread->counter_a = 1;
		
		FILE *handle = NULL;
		char line [MAXLIMIT];
		memset(line, '\0', MAXLIMIT);

		if ((handle = fopen(instance->dork_list, "r")) != NULL) {
			banner();
			say("\n Starting...\n\n");
			while (fgets(line, MAXLIMIT-1, handle)) {
				for (unsigned int a=0; line[a] != '\0'; a++)
					if (line[a] == '\n') {
						line[a] = '\0';
						break;
					}
				
				if (thread->counter_a) {
					param_t *param = (param_t *) xmalloc(sizeof(param_t));
					param->line = (char *) xcalloc( MAXLIMIT + strlen(line) ,sizeof(char) );
					memcpy(param->line, line, strlen(line));
					param->index = thread->counter_c;
					thread->counter_b++;
					thread->counter_a--;
#ifdef WINUSER
					CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) core, (void *) param, 0, 0);
#else 
					pthread_t td;
					pthread_create(&td, NULL, &core, (void *) param);
#endif
				}

				while (1) {
					sleep(10);
					if (thread->counter_a) break;
				}
				
				thread->counter_c++;
				memset(line, '\0', MAXLIMIT);
			}
			fclose(handle);
		}

		while (1) {
			sleep(10);
			if (thread->counter_b == 1) break;
		}
	}
	
	instance->number_of_threads = 0;
	instance->scan_method = 0;
	if (instance->results_domains != NULL)
		free(instance->results_domains);
	if (instance->results_full_url != NULL)
		free(instance->results_full_url);
	if (instance->dork_list != NULL)
		free(instance->dork_list);
	if (instance->top_level != NULL)
		free(instance->top_level);
	if (instance->path_filter != NULL)
		free(instance->path_filter);
	if (instance->data_filter != NULL)
		free(instance->data_filter);
	if (instance->search_engine != NULL)
		free(instance->search_engine);
	if (instance)
		free(instance);

	free(statistics);
	free(thread);
	
#ifdef WINUSER
	WSACleanup();
#endif
	
	return 0;
}

static void *core (void *tparam) {
	param_t *param = (param_t *) tparam;
	int one = 0, two = 0, three = 0, four = 0;
	
	if (instance->search_engine != NULL) {
		if (strstr(instance->search_engine, "1")) one = 1;
		if (strstr(instance->search_engine, "2")) two = 1;
		if (strstr(instance->search_engine, "3")) three = 1;
		if (strstr(instance->search_engine, "4")) four = 1;
	}
	
	if (one == 0 && two == 0 && three == 0 && four == 0) {
		one = 1;
		two = 1;
		three = 1;
		four = 1;
	}
	
	if (one == 1) {
		say("  [%d] -> Dorking: %s - Search engine: Bing\n", param->index, param->line);
		bing(param->line);
	}
	
	if (two == 1) {
		say("[%d] -> Dorking: %s - Search engine: Google\n", param->index, param->line);
		google(param->line);
	}
	
	if (three == 1) {
		say("[%d] -> Dorking: %s - Search engine: Hotbot\n", param->index, param->line);
		hotbot(param->line);
	}
	
	if (four == 1) {
		say("[%d] -> Dorking: %s - Search engine: DuckDuckGo\n", param->index, param->line);
		duckduckgo(param->line);
	}
	
	thread->counter_a++;
	thread->counter_b--;

	free(param->line);
	free(param);

	return NULL;

}

static void google (const char *dork) {
	;
}

static void hotbot (const char *dork) {
	;
}

static void duckduckgo (const char *dork) {
	;
}

static void bing (const char *dork) {
	if (!dork) return;
	
	for (unsigned int page=0; page<1000; page++) {
		int sock = (int) (-1), result = (int) (-1);
		struct sockaddr_in st_addr;
		struct hostent *st_hostent;
		
		if ((st_hostent = gethostbyname("www.bing.com")) == NULL) {
			say("Bing - gethostbyname failed.\n");
			return;
		}
		
		st_addr.sin_family		= AF_INET;
		st_addr.sin_port		= htons(80);
		st_addr.sin_addr.s_addr	= *(u_long *) st_hostent->h_addr_list[0];
		
		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			say("Bing - socket failed.\n");
			return;
		}
		
		if ((result = connect(sock, (struct sockaddr *)&st_addr, sizeof(st_addr))) < 0) {
			say("Bing - connect failed.\n");
			close(sock);
			return;
		}
		
		char header [] = 
			"Host: www.bing.com\r\n"
			"User-Agent: Mozilla/5.0 (Windows NT 6.1; Trident/7.0; SLCC2; "
			".NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; "
			"Media Center PC 6.0; .NET4.0C; .NET4.0E; rv:11.0) like Gecko\r\n"
			"Accept-Encoding: identity\r\n"
			"Connection: Close\r\n\r\n";

		char *final_dork = (char *) xcalloc( (MAXLIMIT + strlen(dork)), sizeof(char) );
		memcpy(final_dork, dork, strlen(dork));
		
		for (int a=0; final_dork[a]!='\0'; a++)
			if (final_dork[a] == ' ')
				final_dork[a] = '+';
		
		char *header_final = (char *) xcalloc((strlen(header) + MAXLIMIT) , sizeof(char) );
		sprintf(header_final, "GET /search?q=%s&first=%d1&FORM=PERE HTTP/1.1\r\n%s", final_dork, page, header);
		
		if (send(sock, header_final, strlen(header_final), 0) == -1) {
			say("Bing - send failed.\n");
			free(header_final);
			free(final_dork);
			close(sock);
			return;
		}
		
		result = 0;
		unsigned int is_going = 1;
		unsigned int total_length = 0;
		char *response = (char *) xcalloc((MAXLIMIT*2), sizeof(char));
		char *response_final = (char *) xcalloc( (MAXLIMIT*2), sizeof(char));
		
		while (is_going) {
			result = recv(sock, response, (sizeof(char) * (MAXLIMIT*2)) - 1, 0);
			if (result == 0 || result < 0)
				is_going = 0;
			else {
				if ((response_final = (char *) realloc(response_final, total_length + 
					(sizeof(char) * (MAXLIMIT*2)))) != NULL) {
					memset(&response_final[total_length], '\0', sizeof(char) * (MAXLIMIT*2));
					memcpy(&response_final[total_length], response, result);
					total_length += result;
				}
			}
		}
		
		close(sock);
		
		unsigned int status_end = FALSE;
		if (total_length > 0 && response_final != NULL)
			if (strlen(response_final) > 0 && strstr(response_final, "class=\"sb_pagN\"")) {
				extract_urls_and_domains(response_final);
			} else status_end = TRUE;
		
		free(header_final);
		free(final_dork);
		free(response);
		free(response_final);
		
		if (status_end == TRUE)
			break;
	}
}

static void extract_urls_and_domains (char *content) {
	if (!content)
		return;
	
	char *pointer = content;
	while (1) {
		pointer = strstr(pointer += 7, "://");
		if (!pointer) break;
		
		unsigned int a = 0;
		for (a=0; pointer[a]!='\0'; a++)
			if (pointer[a] == 0x22 || pointer[a] == '>' || pointer[a] == ' ') 
				break;
		
		unsigned int protocol = 2;
		if (a>0) {
			pointer -= 1;
			if (pointer[0] == 's' || pointer[0] == 'p') {
				pointer -= 1;
				if (pointer[1] == 'p') {
					pointer -= 2;
					if (pointer[2] == 't' && pointer[1] == 't' && pointer[0] == 'h')
						protocol = 0;
				} else if (pointer[1] == 's') {
					pointer -= 3;
					if (pointer[3] == 'p' && pointer[2] == 't' && pointer[1] == 't' && pointer[0] == 'h')
						protocol = 1;
				}
			}
			
			if (protocol == 0 || protocol == 1) {
				if (protocol == 0)
					a += 4;
				else
					a += 5;
				
				char *url = (char *) xcalloc((a + 1), sizeof(char));
				memcpy(url, pointer, a);
				
				if (url)
					if (!(strlen(url) == 7 || strlen(url) == 8))
						filter_url(url, protocol);
					
				free(url);
			}
		}
	}
}

static void filter_url (const char *url, const unsigned int protocol) {
	if (!url)
		return;
	
	if (!strstr(url, "microsofttranslator.com") && 
		!strstr(url, "live.com") &&
		!strstr(url, "microsoft.com") &&
		!strstr(url, "bingj.com") &&
		!strstr(url, "w3.org") &&
		!strstr(url, "wikipedia.org") &&
		!strstr(url, "google.com") &&
		!strstr(url, "yahoo.com") &&
		!strstr(url, "msn.com") &&
		!strstr(url, "facebook.com") &&
		!strstr(url, "bing.net") &&
		!strstr(url, "\n") &&
		!strstr(url, "<") &&
		strstr(url, ".")) 
	{	
		char *domain = (char *) xcalloc(strlen(url), sizeof(char));
		char *pointer = strstr(url, "://");
		unsigned int a = 0;
		
		pointer += strlen("://");
		for (a=0; pointer[a]!='\0'; a++)
			if (pointer[a] == '/' || pointer[a] == ' ') {
				a++;
				break;
			}
		
		unsigned int top_level_enabled = FALSE;
		unsigned int tdl_is_valid = FALSE;
		memcpy(domain, pointer, a);
		domain[a-1] = '/';
		
		/* TDL, save-mode, path and data filter. */
#define FILTER_CODE_BLOCK_A\
		if (strstr(domain, temporary)) {\
			domain[strlen(domain) - 1] = '\0';\
			if (domain_exists(domain) == FALSE) {\
				if (instance->scan_method == 1) {\
					if (path_and_data_filter(domain, protocol) == TRUE) {\
						save(instance->results_domains, domain);\
						say("   + %d -> %s\n", statistics->total_sites, domain);\
						statistics->total_sites++;\
					}\
				} else if (instance->scan_method == 2) {\
					if (path_and_data_filter(domain, protocol) == TRUE) {\
						save(instance->results_full_url, url);\
						say("   + %d -> %s\n", statistics->total_sites, domain);\
						statistics->total_sites++;\
					}\
				} else {\
					if (path_and_data_filter(domain, protocol) == TRUE) {\
						save(instance->results_domains, domain);\
						save(instance->results_full_url, url);\
						say("   + %d -> %s\n", statistics->total_sites, domain);\
						statistics->total_sites++;\
					}\
				}\
				statistics->total_sites_all++;\
			}\
		}
		
		if (instance->top_level != NULL) {
			char temporary [MAXLIMIT];
			if (strstr(instance->top_level, ",")) {
				for (int a=0,b=0; ; a++,b++) {
					if (instance->top_level[a] == ',' || instance->top_level[a] == '\0') {
						temporary[b] = '/';
						temporary[b+1] = '\0';
						b = 0;
						a++;
						FILTER_CODE_BLOCK_A
						if (instance->top_level[a] == '\0')
							break;
					}
					temporary[b] = instance->top_level[a];
				}
				top_level_enabled = TRUE;
			}
		}
		
		if (top_level_enabled == FALSE) {
			char temporary [MAXLIMIT];
			unsigned int flag_control = FALSE;
			
			if (instance->top_level != NULL) {
				if (!strstr(instance->top_level, ",")) 
					sprintf(temporary, "%s/", instance->top_level);
				else flag_control = TRUE;
			} else flag_control = TRUE;
			if (flag_control == TRUE) 
				sprintf(temporary, "/");
			FILTER_CODE_BLOCK_A
		}
		
		free(domain);
	}
}

static unsigned int path_and_data_filter (const char *domain, const unsigned int protocol) {
	if (!domain)
		return FALSE;
	
	unsigned int result = FALSE;
	char path [MAXLIMIT*2];
	char save_buffer [MAXLIMIT*2];
	
	if (instance->path_filter != NULL) {
		 char temporary [MAXLIMIT];
		if (strstr(instance->path_filter, ",")) {
			for (int a=0,b=0; ; a++,b++) {
				if (instance->path_filter[a] == ',' || instance->path_filter[a] == '\0') {
					
					temporary[b] = '\0';
					b = 0; a++;
					http_request_t *request = http_request_get(domain, temporary);
					
					if (request != NULL) {
						if (request->e_200_OK == TRUE && instance->data_filter == NULL) {
							sprintf(save_buffer, "%s%s%s", (protocol) ? "https://" : "http://",  domain, temporary);
							sprintf(path, "%s-paths.txt", instance->dork_list);
							save(path, save_buffer);
							result = TRUE;
						} 
						else if (request->e_200_OK == TRUE && instance->data_filter != NULL) {
							if (strstr(request->content, instance->data_filter)) {
								sprintf(save_buffer, "%s%s%s", (protocol) ? "https://" : "http://", domain, temporary);
								sprintf(path, "%s-paths.txt", instance->dork_list);
								save(path, save_buffer);
								result = TRUE;
							}
							if (request->content)
								free(request->content);	
						}
						
						free(request);
					}
					
					if (instance->path_filter[a] == '\0')
						break;
				}
				temporary[b] = instance->path_filter[a];
			}
		} else {
			http_request_t *request = http_request_get(domain, instance->path_filter);
			if (request != NULL) {
				if (request->e_200_OK == TRUE && instance->data_filter == NULL) {
					sprintf(save_buffer, "%s%s%s", (protocol) ? "https://" : "http://",  domain, instance->path_filter);
					sprintf(path, "%s-paths.txt", instance->dork_list);
					save(path, save_buffer);
					result = TRUE;
				} 
				else if (request->e_200_OK == TRUE && instance->data_filter != NULL) {
					if (strstr(request->content, instance->data_filter)) {
						sprintf(save_buffer, "%s%s%s", (protocol) ? "https://" : "http://", domain, instance->path_filter);
						sprintf(path, "%s-paths.txt", instance->dork_list);
						save(path, save_buffer);
						result = TRUE;
					}
					if (request->content)
						free(request->content);	
				}
				
				free(request);
			}
		}
	} else result = TRUE;
	
	return result;
}

static http_request_t *http_request_get (const char *domain, const char *path) {
	if (!domain)
		return (http_request_t *) NULL;
	
	int sock = (int) (-1), result = (int) (-1);
	struct sockaddr_in st_addr;
	struct hostent *st_hostent;
	
	if ((st_hostent = gethostbyname(domain)) == NULL) {
		say("HTTP GET - gethostbyname failed.\n");
		return (http_request_t *) NULL;
	}
	
	st_addr.sin_family		= AF_INET;
	st_addr.sin_port		= htons(80);
	st_addr.sin_addr.s_addr	= *(u_long *) st_hostent->h_addr_list[0];
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		say("HTTP GET - socket failed.\n");
		return (http_request_t *) NULL;
	}
	
	if ((result = connect(sock, (struct sockaddr *)&st_addr, sizeof(st_addr))) < 0) {
		say("HTTP GET - connect failed.\n");
		close(sock);
		return (http_request_t *) NULL;
	}
	
	char header [] = 
		"User-Agent: Mozilla/5.0 (Windows NT 6.1; Trident/7.0; SLCC2; "
		".NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; "
		"Media Center PC 6.0; .NET4.0C; .NET4.0E; rv:11.0) like Gecko\r\n"
		"Accept-Encoding: identity\r\n"
		"Connection: Close\r\n\r\n";

	char *header_final = ( char *) xcalloc( (strlen(header) + MAXLIMIT), sizeof( char) );
	sprintf(header_final, "GET %s HTTP/1.1\r\nHost: %s\r\n%s", path, domain, header);
	
	if (send(sock, header_final, strlen(header_final), 0) == -1) {
		say("HTTP GET - send failed.\n");
		free(header_final);
		close(sock);
		return (http_request_t *) NULL;
	}
	
	result = 0;
	unsigned int is_going = 1;
	unsigned int total_length = 0;
	char *response = (char *) xcalloc( (MAXLIMIT*2), sizeof(char));
	char *response_final = (char *) xcalloc( (MAXLIMIT*2), sizeof(char));
	
	while (is_going) {
		result = recv(sock, response, (sizeof(char) * MAXLIMIT) - 1, 0);
		if (result == 0 || result < 0)
			is_going = 0;
		else {
			if (!strstr(response, "\r\n\r\n") && instance->data_filter == NULL) {
				if ((response_final = (char *) realloc(response_final, total_length + 
					(sizeof(char) * MAXLIMIT))) != NULL) {
					memset(&response_final[total_length], '\0', sizeof(char) * MAXLIMIT);
					memcpy(&response_final[total_length], response, result);
					total_length += result;
				}
			} else if (instance->data_filter != NULL) {
				if ((response_final = ( char *) realloc(response_final, total_length + 
					(sizeof( char) * MAXLIMIT))) != NULL) {
					memset(&response_final[total_length], '\0', sizeof(char) * MAXLIMIT);
					memcpy(&response_final[total_length], response, result);
					total_length += result;
				}
			} 
			else 
				break;
		}
	}
	
	close(sock);
	http_request_t *request = (http_request_t *) xmalloc(sizeof(http_request_t));
	if (total_length > 0 && response_final != NULL) {
		if (instance->data_filter != NULL) {
			request->length = total_length;
			request->content = (char *) xcalloc( (total_length + 1) , sizeof(char));
			memcpy(request->content, response_final, total_length);
		}
		
		if (strlen(response_final) > 0 && strstr(response_final, "200 OK")) {
			request->e_200_OK = TRUE;
		}
	}
		
	free(header_final);
	free(response);
	free(response_final);
	
	return request;
}

static unsigned int domain_exists (const char *domain) {
	if (!domain)
		return FALSE;
	FILE *handle = NULL;
	char line [MAXLIMIT];
	unsigned int flag_exists = FALSE;
	memset(line, '\0', MAXLIMIT);
	if ((handle = fopen(instance->results_domains, "r")) != NULL) {
		while (fgets(line, MAXLIMIT-1, handle)) {
			if (strstr(line, domain)) {
				flag_exists = TRUE;
				break;
			}
			memset(line, '\0', MAXLIMIT);
		}
		fclose(handle);
	}
	if (flag_exists == TRUE)
		return TRUE;
	return FALSE;
}

static void save (const char *path, const char *content) {
	if (!path || !content)
		return;
	FILE *handle = NULL;
	if ((handle = fopen(path, "a+")) != NULL) {
		fprintf(handle, "%s\n", content);
		fclose(handle);
	}
}

static unsigned int file_exists (const char *path) {
	if (!path)
		return FALSE;
	FILE *handle = NULL;
	if ((handle = fopen(path, "r")) != NULL) {
		fclose(handle);
		return TRUE;
	}
	return FALSE;
}

void *xmalloc (const unsigned int size) {
	if (!size)
		return NULL;
	void *pointer = NULL;
	if ((pointer = malloc(size)) != NULL) 
		return pointer;
	return NULL;
}

void *xcalloc (const unsigned int nmemb, const unsigned int size) {
	if (!size)
		return NULL;
	void *pointer = NULL;
	if ((pointer = calloc(nmemb, size)) != NULL) 
		return pointer;
	return NULL;
}

static void die (const char *content, const unsigned int exitCode) {
	say("%s", content);
	exit(exitCode);
}

static void banner (void) {
	say("\n\
     __                       __                 __                      \n\
    /\\ \\       __            /\\ \\               /\\ \\                     \n\
    \\ \\ \\     /\\_\\    ____   \\_\\ \\    ___   _ __\\ \\ \\/'\\      __   _ __  \n\
     \\ \\ \\  __\\/\\ \\  /',__\\  /'_` \\  / __`\\/\\\\`'__\\ \\ , <   /'__`\\/\\`'__\\\n\
      \\ \\ \\L\\ \\\\ \\ \\/\\__, `\\/\\ \\L\\ \\/\\ \\L\\ \\ \\ \\/ \\ \\ \\\\`\\ /\\  __/\\ \\ \\/ \n\
       \\ \\____/ \\ \\_\\/\\____/\\ \\___,_\\ \\____/\\ \\_\\  \\ \\_\\ \\_\\ \\____\\\\ \\_\\ \n\
        \\/___/   \\/_/\\/___/  \\/__,_ /\\/___/  \\/_/   \\/_/\\/_/\\/____/ \\/_/ \n\
                                                                     \n\
                        My GitHub: github.com/nullrndtx\n\
                    Dracos Github: github.com/dracos-linux\n\n");
}

static void help (void) {
	say("\n  -l  -  List of dorks.\n\
  -t  -  Number of threads (Default: 1).\n\
  -v  -  Top-level filter.\n\
  -p  -  Path to search (Default: 200 OK HTTP error).\n\
      '-> Output path file: DORK_FILE_NAME-paths.txt\n\
  -d  -  Data to search.\n\
  -c  -  Search engine (Default: all).\n\
      |-> 1: Bing\n\
      |-> 2: Google - NOT IMPLEMENTED\n\
      |-> 3: Hotbot - NOT IMPLEMENTED\n\
      '-> 4: DuckDuckGo - NOT IMPLEMENTED\n\
  -m  -  Save method (Default: all).\n\
      |-> 1: Extract domains (DORK_FILE_NAME-domains.txt).\n\
      '-> 2: Extract full URL (DORK_FILE_NAME-full_url.txt).\n\n\
  Examples...\n\
    dorker -l dorks.txt -t 30 -m 1 -v .com.br\n\
    dorker -l dorks.txt -t 30 -m 2 -v .gov\n\
    dorker -l dorks.txt  -t 30 -v .gov,.com,.com.br\n\
    dorker -l dorks-bing.txt -t 30 -m 1 -c 1 -v .br\n\
    -p /wp-login.php,/wp-admin.php,/wordpress/wp-login.php -d \"wp-submit\"\n\
    dorker -l dorks-all.txt -t 100 -c 12\n");
}
