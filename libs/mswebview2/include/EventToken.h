#ifndef EVENT_TOKEN_H
#define EVENT_TOKEN_H

// MinGW does not ship eventtoken.h, so define EventRegistrationToken here.
typedef __INT64_TYPE__ EventRegistrationToken;

#endif  // EVENT_TOKEN_H
