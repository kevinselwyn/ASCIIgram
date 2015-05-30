#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <curses.h>
#include <curl/curl.h>
#include <jansson.h>
#include <math.h>
#include <jpeglib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>

#define FALSE   0
#define TRUE    1
#define PORT    1337
#define BUFSIZE 1024
#define MAX     256

#define API_VERSION  "v1"
#define API_ROOT     "https://api.instagram.com"
#define API_AUTH     API_ROOT "/oauth/authorize/"
#define API_TOKEN    API_ROOT "/oauth/access_token"
#define API_FEED     API_ROOT "/" API_VERSION "/users/self/feed"
#define API_LIKE     API_ROOT "/" API_VERSION "/media"
#define API_REDIRECT "http://localhost:1337/"

#ifndef _OPEN
	#ifndef __APPLE__
		#define _OPEN "xdg-open"
	#else
		#define _OPEN "open"
	#endif
#endif

int width = 0, height = 0, left = 0, container = 37;
int photo_pos = 0, photo_count = 0;
json_t *photos[MAX];

struct curl_response {
	char *memory;
	size_t size;
};

static size_t get_write(void *ptr, size_t size, size_t nmemb, char *res) {
	size_t real_size = size * nmemb;
	struct curl_response *mem = (struct curl_response *)res;

	mem->memory = realloc(mem->memory, mem->size + real_size + 1);

	if (mem->memory == NULL) {
		printf("Memory error\n");

		return 0;
	}

	memcpy(&(mem->memory[mem->size]), ptr, real_size);
	mem->size += real_size;
	mem->memory[mem->size] = (char)0;

	return real_size;
}

static void get_dims() {
	struct winsize w;

	ioctl(0, TIOCGWINSZ, &w);

	width = w.ws_col;
	height = w.ws_row;
	left = floor((width - container) / 2);
}

static void draw_init() {
	initscr();
	noecho();
	curs_set(0);
	nodelay(stdscr, TRUE);
	leaveok(stdscr, TRUE);
	scrollok(stdscr, FALSE);

	get_dims();
}

static void draw_clear() {
	int x = 0, y = 0;

	for (x = 1, y = 1; y <= height; y++) {
		printf("\033[%d;%dH%*s", y, x, width, " ");
	}
}

static void draw_update() {
	doupdate();
}

static void draw_end() {
	endwin();
}

static void draw_border() {
	int i = 0, l = 0, edge = 0;
	char *borders[7] = { "─", "│", "┌", "┐", "┘", "└", "┬" };

	/* Draw Main Corners */
	printf("\033[%d;%dH%s", 1, 1, borders[2]);
	printf("\033[%d;%dH%s", 1, width, borders[3]);
	printf("\033[%d;%dH%s", height, width, borders[4]);
	printf("\033[%d;%dH%s", height, 1, borders[5]);

	/* Draw Photo Corners */
	printf("\033[%d;%dH%s", 21, left + container + 1, borders[4]);
	printf("\033[%d;%dH%s", 21, left, borders[5]);
	printf("\033[%d;%dH%s", 1, left, borders[6]);
	printf("\033[%d;%dH%s", 1, left + container + 1, borders[6]);

	/* Draw Top Left */
	edge = (int)floor(((width - 2) - (container + 2)) / 2);

	for (i = 0, l = edge; i < l; i++) {
		printf("\033[%d;%dH%s", 1, i + 2, borders[0]);
	}

	/* Draw Top Right */
	edge = ((width - 2) - (container + 2)) - edge;

	for (i = width - edge, l = width; i < l; i++) {
		printf("\033[%d;%dH%s", 1, i, borders[0]);
	}

	/* Draw Main Bottom */
	for (i = 0, l = width - 2; i < l; i++) {
		printf("\033[%d;%dH%s", height, i + 2, borders[0]);
	}

	/* Draw Photo Top */
	for (i = left + 1, l = left + 12 + 1; i < l; i++) {
		printf("\033[%d;%dH%s", 1, i, borders[0]);
	}

	for (i = left + container - 11, l = left + container + 1; i < l; i++) {
		printf("\033[%d;%dH%s", 1, i, borders[0]);
	}

	/* Draw Photo Bottom */
	for (i = left + 1, l = left + container + 1; i < l; i++) {
		printf("\033[%d;%dH%s", 21, i, borders[0]);
	}

	/* Draw Main Left */
	for (i = 0, l = height - 1; i < l; i++) {
		printf("\033[%d;%dH%s", i + 2, 1, borders[1]);
	}

	/* Draw Main Right */
	for (i = 0, l = height - 1; i < l; i++) {
		printf("\033[%d;%dH%s", i + 2, width, borders[1]);
	}

	/* Draw Photo Left */
	for (i = 0, l = 19; i < l; i++) {
		printf("\033[%d;%dH%s", i + 2, left, borders[1]);
	}

	/* Draw Photo Right */
	for (i = 0, l = 19; i < l; i++) {
		printf("\033[%d;%dH%s", i + 2, left + container + 1, borders[1]);
	}

	/* Draw Title */
	printf("\033[%d;%dH%s", 1, left + 13, "┤ ASCIIgram ├");
}

static char findchar(int sample, double sample_mean, double mean, double sigma) {
	char ascii_chars[14] = { ' ', '.', '*', '@', 'O', 'o', '`', ',', '^', '[', ']', '=', '\\', '/' };

	switch (sample) {
	case 0b0000:
		if ((sample_mean >= mean + (sigma * 1)) || sample_mean >= 255) {
			return ascii_chars[0];
		} else if (sample_mean >= mean + (0.3 * sigma)) {
			return ascii_chars[1];
		} else if (sample_mean >= mean) {
			return ascii_chars[2];
		}
	case 0b1111:
		if ((sample_mean <= mean - (sigma * 1)) || sample_mean <= 0) {
			return ascii_chars[3];
		} else if (sample_mean <= mean - (0.3 * sigma)) {
			return ascii_chars[4];
		} else if (sample_mean <= mean) {
			return ascii_chars[5];
		}
		break;
	case 0b1000:
	case 0b0010:
		return ascii_chars[6];
	case 0b0100:
	case 0b0001:
		return ascii_chars[7];
		break;
	case 0b1010:
		return ascii_chars[8];
		break;
	case 0b1100:
		return ascii_chars[9];
		break;
	case 0b0011:
		return ascii_chars[10];
		break;
	case 0b0101:
		return ascii_chars[11];
		break;
	case 0b1001:
	case 0b1011:
	case 0b1101:
		return ascii_chars[12];
		break;
	case 0b0110:
	case 0b0111:
	case 0b1110:
		return ascii_chars[13];
		break;
	default:
		break;
	}

	return ascii_chars[0];
}

static void jpeg2ascii(char *url, char **ascii) {
	int i = 0, j = 0, l = 0, x = 0, y = 0, r = 0, g = 0, b = 0;
	int width = 0, height = 0;
	int row_stride = 0, count = 0, counter = 0, offset = 0, sigma = 0, sample = 0;
	double yiq = 0.0, yiq_sum = 0.0, yyy = 0.0;
	double mean = 0.0, sample_mean = 0.0;
	char *result = NULL;
	unsigned char *jpegdata = NULL, *yiqs = NULL;
	struct jpeg_decompress_struct cinfo_decompress;
	struct jpeg_error_mgr jerr;
	struct curl_response response;
	FILE *jpegfile = NULL;
	JSAMPARRAY pJpegBuffer;
	CURL *curl;
	CURLcode res;

	response.memory = malloc(1);
	response.size = 0;

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_write);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

		res = curl_easy_perform(curl);

		if (res != CURLE_OK) {
			printf("Error: %s\n", curl_easy_strerror(res));
			goto cleanup;
		}
	}

	cinfo_decompress.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&cinfo_decompress);
	jpeg_mem_src(&cinfo_decompress, (unsigned char *)response.memory, response.size);

	jpeg_read_header(&cinfo_decompress, TRUE);
	jpeg_start_decompress(&cinfo_decompress);

	width = cinfo_decompress.output_width;
	height = cinfo_decompress.output_height;
	row_stride = width * cinfo_decompress.output_components;

	pJpegBuffer = (*cinfo_decompress.mem->alloc_sarray)((j_common_ptr) &cinfo_decompress, JPOOL_IMAGE, row_stride, 1);

	jpegdata = malloc(sizeof(char) * (width * height * 3) + 1);
	yiqs = malloc(sizeof(char) * (height * width + width) + 1);

	result = malloc(sizeof(char) * (width * height) + 1);
	strcpy(result, "");

	count = 0;
	counter = 0;
	y = 0;

	while (cinfo_decompress.output_scanline < cinfo_decompress.output_height) {
		jpeg_read_scanlines(&cinfo_decompress, pJpegBuffer, 1);

		for (x = 0; x < width; x++) {
			r = pJpegBuffer[0][cinfo_decompress.output_components * x];
			g = pJpegBuffer[0][cinfo_decompress.output_components * x + 1];
			b = pJpegBuffer[0][cinfo_decompress.output_components * x + 2];

			jpegdata[counter] = r;
			jpegdata[counter + 1] = g;
			jpegdata[counter + 2] = b;

			offset = y * width + x;
			yiq = ((r * 0.299) + (g * 0.587) + (b * 0.114));
			yiqs[offset] = yiq;
			yiq_sum += yiq;

			count++;
			counter += 3;
		}

		y++;
	}

	mean = yiq_sum / count;
	sigma = 0;

	for (i = 0, l = height * width + width; i < l; i++) {
		if (yiqs[i] != '\0') {
			sigma += pow(yiqs[i] - mean, 2);
		}
	}

	sigma = sqrt(sigma / count);
	count = 0;

	for (y = 0; y < height - 2; y += 8) {
		for (x = 0; x < width - 2; x += 4) {
			sample = 0;
			sample_mean = 0.0;

			for (i = y; i < y + 2; i++) {
				for (j = x; j < x + 2; j++) {
					r = jpegdata[((i * width) + j) * 3];
					g = jpegdata[((i * width) + j) * 3 + 1];
					b = jpegdata[((i * width) + j) * 3 + 2];

					yyy = yiqs[i * width + j];

					if (yyy < mean) {
						sample |= 0b0001;
					}

					sample <<= 1;
					sample_mean += yyy;
				}
			}

			sample_mean /= 4;
			sample >>= 1;

			result[count++] = findchar(sample, sample_mean, mean, sigma);
		}

		result[count++] = '\n';
	}

	result[count] = '\0';

	*ascii = malloc(sizeof(char) * strlen(result) + 1);
	strcpy(*ascii, result);

cleanup:
	if (jpegfile) {
		fclose(jpegfile);
	}

	if (jpegdata) {
		free(jpegdata);
	}

	if (yiqs) {
		free(yiqs);
	}

	if (result) {
		free(result);
	}
}

static void draw_photo(char **id) {
	int photo_width = 0, photo_lines = 0;
	int i = 0, l = 0, x = 0, y = 0;
	size_t photo_length = 0;
	char *url = NULL, *ascii = NULL, *line = NULL;
	const char *tmp = NULL;
	json_t *photo, *id_node, *images_node, *thumbnail_node, *url_node;

	photo = photos[photo_pos];

	/* ID */
	id_node = json_object_get(photo, "id");
	tmp = json_string_value(id_node);
	*id = malloc(sizeof(char) * (strlen(tmp) + 1));
	strcpy(*id, tmp);

	/* Image */
	images_node = json_object_get(photo, "images");
	thumbnail_node = json_object_get(images_node, "thumbnail");
	url_node = json_object_get(thumbnail_node, "url");
	tmp = json_string_value(url_node);
	url = malloc(sizeof(char) * (strlen(tmp) + 1));
	strcpy(url, tmp);

	jpeg2ascii(url, &ascii);

	x = left + 1;
	y = 2;

	photo_length = strlen(ascii);

	for (i = 0, l = (int)photo_length; i < l; i++) {
		if (ascii[i] == '\n') {
			photo_width = i;
			break;
		}
	}

	line = malloc(sizeof(char) * (photo_width + 1));
	photo_lines = photo_length / photo_width;

	for (i = 0, l = photo_lines; i < l; i++) {
		strncpy(line, ascii + ((photo_width + 1) * i), photo_width);
		line[photo_width] = '\0';
		printf("\033[%d;%dH%s", y++, x, line);
	}

	if (url) {
		free(url);
	}

	if (line) {
		free(line);
	}
}

static void draw_username() {
	char *username = NULL;
	const char *tmp = NULL;
	json_t *photo, *user_node, *username_node;

	photo = photos[photo_pos];

	user_node = json_object_get(photo, "user");
	username_node = json_object_get(user_node, "username");
	tmp = json_string_value(username_node);
	username = malloc(sizeof(char) * (strlen(tmp) + 1));
	strcpy(username, tmp);

	printf("\033[%d;%dH%*s", 23, left + 1, container, " ");
	printf("\033[%d;%dH%s", 23, left + 1, username);

	if (username) {
		free(username);
	}
}

static void draw_likes() {
	int likes = 0, liked = 0, liked_padding = 0;
	char *liked_str = NULL;
	json_t *photo, *likes_node, *liked_node, *count_node;

	photo = photos[photo_pos];

	likes_node = json_object_get(photo, "likes");
	count_node = json_object_get(likes_node, "count");
	likes = json_integer_value(count_node);

	liked_padding = 7;
	liked_node = json_object_get(photo, "user_has_liked");
	liked = json_is_true(liked_node);
	liked_str = malloc(sizeof(char) * 11);

	if (liked) {
		liked_padding -= 4;
		sprintf(liked_str, "\x1B[31m♥\x1B[0m %d", likes);
	} else {
		sprintf(liked_str, "♥ %d", likes);
	}

	printf("\033[%d;%dH%10s", 23, left + container - liked_padding, liked_str);

	if (liked_str) {
		free(liked_str);
	}
}

static void draw_video() {
	char *type = NULL;
	const char *tmp = NULL;
	json_t *photo, *type_node;

	photo = photos[photo_pos];

	type_node = json_object_get(photo, "type");
	tmp = json_string_value(type_node);
	type = malloc(sizeof(char) * (strlen(tmp) + 1));
	strcpy(type, tmp);

	if (strcmp(type, "video") == 0) {
		printf("\033[%d;%dH%s", 22, left + container, "▶");
	} else {
		printf("\033[%d;%dH%s", 22, left + container, " ");
	}

	if (type) {
		free(type);
	}
}

static void draw_link(char **link) {
	const char *tmp = NULL;
	json_t *photo, *link_node;

	photo = photos[photo_pos];

	if (*link) {
		free(*link);
	}

	link_node = json_object_get(photo, "link");
	tmp = json_string_value(link_node);
	*link = malloc(sizeof(char) * (strlen(tmp) + 1));
	strcpy(*link, tmp);
}

static void draw_instagram(char **id, char **link) {
	draw_clear();

	draw_photo(&*id);
	draw_username();
	draw_likes();
	draw_video();
	draw_link(&*link);

	draw_update();
}

static int open_url(char *url) {
	int rc = 0;
	size_t open_cmd_length = 0;
	char *open = NULL, *open_cmd = NULL;

	open = _OPEN;

	open_cmd_length = strlen(open) + strlen(url) + 16;
	open_cmd = malloc(sizeof(char) * (open_cmd_length + 1));
	sprintf(open_cmd, "%s \"%s\" &> /dev/null", open, url);

	rc = system(open_cmd);

	if (open_cmd) {
		free(open_cmd);
	}

	return rc;
}

static int get_access_token(char **access_token, char *client_id, char *client_secret) {
	int rc = 0, port = PORT, create_socket = 0, new_socket = 0, i = 0, l = 0;
	size_t auth_url_length = 0;
	size_t bufsize = BUFSIZE, code_length = 0, postfields_length = 0;
	char *data = NULL, *auth_root = NULL, *auth_token = NULL;
	char *auth_redirect = NULL, *auth_url = NULL;
	char *buffer = NULL, *code = NULL, *postfields = NULL;
	const char *tmp = NULL;
	socklen_t addrlen;
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = INADDR_ANY,
		.sin_port = htons(port)
	};
	struct curl_response response;
	CURL *curl;
	CURLcode res;
	json_t *root, *access_token_node;
	json_error_t error;

	auth_root = API_AUTH;
	auth_token = API_TOKEN;
	auth_redirect = API_REDIRECT;

	auth_url_length = strlen(auth_root) + strlen(client_id) + strlen(auth_redirect) + 44;
	auth_url = malloc(sizeof(char) * (auth_url_length + 1));
	sprintf(auth_url, "%s?client_id=%s&redirect_uri=%s&response_type=code", auth_root, client_id, auth_redirect);

	buffer = malloc(bufsize);

	if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		printf("Socket could not be created\n");

		rc = 1;
		goto cleanup;
	}

	if (bind(create_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		printf("Could not bind socket\n");

		rc = 1;
		goto cleanup;
	}

	if (open_url(auth_url) != 0) {
		printf("Please open:\n%s\n", auth_url);
	}

	while (1 == TRUE) {
		if (listen(create_socket, 10) < 0) {
			printf("Server: listen\n");

			rc = 1;
			goto cleanup;
		}

		if ((new_socket = accept(create_socket, (struct sockaddr *)&addr, &addrlen)) < 0) {
			printf("Server: accept\n");

			rc = 1;
			goto cleanup;
		}

		if (new_socket == 0) {
			printf("The client could not connect\n");

			rc = 1;
			goto cleanup;
		}

		recv(new_socket, buffer, bufsize, 0);

		if (strncmp(buffer, "GET /?code=", 11) == 0) {
			for (i = 0, l = (int)bufsize; i < l; i++) {
				if (buffer[i] == '\n') {
					code_length = (size_t)i;
					break;
				}
			}

			write(new_socket, "HTTP/1.1 200 OK\nContent-length: 18\nContent-Type: text/html\n\n<pre>Success</pre>", 78);
			close(new_socket);

			break;
		} else {
			write(new_socket, "", 0);
			close(new_socket);

			continue;
		}
	}

	code_length -= 21;
	code = malloc(sizeof(char) * (code_length + 1));
	strncpy(code, buffer + 11, code_length);
	code[code_length] = '\0';

	response.memory = malloc(1);
	response.size = 0;

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	if (curl) {
		postfields_length = strlen(client_id) + 10;
		postfields_length += strlen(client_secret) + 15;
		postfields_length += 30;
		postfields_length += strlen(auth_redirect) + 14;
		postfields_length += strlen(code) + 6;

		postfields = malloc(sizeof(char) * (postfields_length + 1));
		sprintf(postfields, "client_id=%s&client_secret=%s&grant_type=authorization_code&redirect_uri=%s&code=%s", client_id, client_secret, auth_redirect, code);

		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
		curl_easy_setopt(curl, CURLOPT_URL, auth_token);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_write);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

		res = curl_easy_perform(curl);

		if (res != CURLE_OK) {
			printf("Error: %s\n", curl_easy_strerror(res));

			rc = 1;
			goto cleanup;
		}

		data = malloc(sizeof(char) * response.size + 1);
		strcpy(data, response.memory);

		curl_easy_cleanup(curl);
	}

	root = json_loads(data, 0, &error);

	if (!json_is_object(root)) {
		printf("Invalid JSON\n");

		rc = 1;
		goto cleanup;
	}

	access_token_node = json_object_get(root, "access_token");
	tmp = json_string_value(access_token_node);

	*access_token = malloc(sizeof(char) * (strlen(tmp) + 1));
	strcpy(*access_token, tmp);

cleanup:
	close(create_socket);
	curl_global_cleanup();

	if (auth_url) {
		free(auth_url);
	}

	if (buffer) {
		free(buffer);
	}

	return rc;
}

static int get_feed(char *access_token, char **max_id) {
	int rc = 0, i = 0, l = 0;
	size_t api_feed_url_length = 0;
	char *api_feed = NULL, *api_feed_url = NULL, *data = NULL, *type = NULL;
	const char *tmp = NULL;
	struct curl_response response;
	CURL *curl;
	CURLcode res;
	json_t *root, *pagination, *meta, *max_id_node, *code, *data_nodes, *data_node, *type_node;
	json_error_t error;

	response.memory = malloc(1);
	response.size = 0;

	api_feed = API_FEED;
	api_feed_url_length = strlen(api_feed) + strlen(access_token) + (*max_id != NULL ? strlen(*max_id) : 0) + 22;

	api_feed_url = malloc(sizeof(char) * (api_feed_url_length + 1));
	sprintf(api_feed_url, "%s?access_token=%s&max_id=%s", api_feed, access_token, *max_id != NULL ? *max_id : "");

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, api_feed_url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_write);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

		res = curl_easy_perform(curl);

		if (res != CURLE_OK) {
			printf("Error: %s\n", curl_easy_strerror(res));

			rc = 1;
			goto cleanup;
		}

		data = malloc(sizeof(char) * (response.size + 1));
		strcpy(data, response.memory);

		curl_easy_cleanup(curl);
	}

	root = json_loads(data, 0, &error);

	if (!json_is_object(root)) {
		printf("Invalid JSON\n");

		rc = 1;
		goto cleanup;
	}

	meta = json_object_get(root, "meta");
	code = json_object_get(meta, "code");

	if (json_integer_value(code) != 200) {
		printf("Unsuccessful request\n");

		rc = 1;
		goto cleanup;
	}

	pagination = json_object_get(root, "pagination");
	max_id_node = json_object_get(pagination, "next_max_id");
	tmp = json_string_value(max_id_node);

	if (*max_id) {
		free(*max_id);
	}

	*max_id = malloc(sizeof(char) * (strlen(tmp) + 1));
	strcpy(*max_id, tmp);

	data_nodes = json_object_get(root, "data");

	for (i = 0, l = json_array_size(data_nodes); i < l; i++) {
		data_node = json_array_get(data_nodes, i);
		type_node = json_object_get(data_node, "type");

		tmp = json_string_value(type_node);
		type = malloc(sizeof(char) * (strlen(tmp) + 1));
		strcpy(type, tmp);

		if (photo_count < MAX) {
			photos[photo_count++] = data_node;
		}
	}

cleanup:
	curl_global_cleanup();

	if (api_feed_url) {
		free(api_feed_url);
	}

	if (data) {
		free(data);
	}

	if (type) {
		free(type);
	}

	if (response.memory) {
		free(response.memory);
	}

	return rc;
}

static void usage(char *exec) {
	printf("Usage: %s --client_id=\"<id>\" --client_secret=\"<secret>\"\n", exec);
}

int main(int argc, char *argv[]) {
	int rc = 0, first = 1, c = 0, i = 0, l = 0;
	size_t length = 0;
	char *exec = NULL, *arg = NULL;
	char *client_id = NULL, *client_secret = NULL, *access_token = NULL;
	char *max_id = NULL, *id = NULL, *link = NULL;

	exec = argv[0];

	if (argc < 2) {
		usage(exec);

		rc = 1;
		goto cleanup;
	}

	for (i = 1, l = argc; i < l; i++) {
		arg = argv[i];
		length = strlen(arg);

		if (strncmp(arg, "--client_id", 11) == 0) {
			length -= 12;
			client_id = malloc(sizeof(char) * (length + 1));
			strncpy(client_id, arg + 12, length);
			client_id[length] = '\0';
		}

		if (strncmp(arg, "--client_secret", 15) == 0) {
			length -= 16;
			client_secret = malloc(sizeof(char) * (length + 1));
			strncpy(client_secret, arg + 16, length);
			client_secret[length] = '\0';
		}
	}

	if (get_access_token(&access_token, client_id, client_secret) != 0) {
		printf("Unable to get access_token\n");

		rc = 1;
		goto cleanup;
	}

	draw_init();

	while (1 == TRUE) {
		c = getch();

		draw_border();

		if ((photo_pos == 0 && first == 1) || photo_pos == photo_count - 1) {
			get_feed(access_token, &max_id);
			draw_instagram(&id, &link);

			if (first == 1) {
				first = 0;
			}
		}

		switch (c) {
		case '\033':
			getch();
			c = getch();

			switch (c) {
			case 'A':
				photo_pos--;

				if (photo_pos < 0) {
					photo_pos = 0;
				}

				break;
			case 'B':
				photo_pos++;

				if (photo_pos >= MAX) {
					goto cleanup;
				}

				break;
			default:
				break;
			}

			draw_instagram(&id, &link);

			break;
		case 'o':
			open_url(link);
			break;
		case 'q':
			goto cleanup;
			break;
		default:
			break;
		}
	}

cleanup:
	draw_clear();
	draw_end();

	if (client_id) {
		free(client_id);
	}

	if (client_secret) {
		free(client_secret);
	}

	if (access_token) {
		free(access_token);
	}

	if (id) {
		free(id);
	}

	if (link) {
		free(link);
	}

	return rc;
}