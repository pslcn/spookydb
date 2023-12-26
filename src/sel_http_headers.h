#ifndef SEL_HTTP_HEADERS_H_
#define SEL_HTTP_HEADERS_H_

#define LIST_OF_SEL_HEADERS \
        X(Host, host) \
        X(User-Agent, user_agent) \
        X(Accept, accept) \
        X(Keep-Alive, keep_alive) \
        X(Cache-Control, cache_control)

struct parsed_req_headers {
#define X(header_name, var_name) struct rw_buff var_name;
        LIST_OF_SEL_HEADERS
#undef X
};

/* Returns idx of request body */
static size_t http_parse_headers(char *req, size_t req_size, size_t start_idx)
{
        size_t next_idx = start_idx;
        size_t header_value_idx, header_crlf_idx;

        while (1) {
                if (req[next_idx] == '\r' && req[next_idx + 1] == '\n') {
                        return next_idx + 2;

                } else {
                        http_get_crlf_idx(req, req_size, next_idx, &header_crlf_idx);

#define X(header_name, var_name) \
                        if (strncmp(&req[next_idx], #header_name, strlen(#header_name) - 1) == 0) { \
                                http_header_value_start(req, req_size, next_idx, &header_value_idx); \
                                memcpy(parsed_req_headers->var_name, &req[header_value_idx], header_crlf_idx - header_value_idx); \
                                parsed_req_headers->var_name[header_crlf_idx - header_value_idx] = '\0'; \
                                next_idx = header_crlf_idx + 2; \
                                continue; \
                        }

                        LIST_OF_SEL_HEADERS
#undef X

                        next_idx = header_crlf_idx + 2;
                }
        }
}

#endif

