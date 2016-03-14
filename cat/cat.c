#include <stdio.h>
#include <unistd.h>

ssize_t read_(int fileDescriptor, void *buf, size_t count) {
	ssize_t result = 0;

	while (result < count) {
		ssize_t cur = read(fileDescriptor, buf + result, count - result);
		
		if (cur == 0)
			break;

		if (cur == -1)
			return -1;

		result += cur;
	}

	return result;
}

ssize_t write_(int fileDescriptor, const void *buf, size_t count) {
	ssize_t result = 0;

	while (result < count) {
		ssize_t cur = write(fileDescriptor, buf + result, count - result);

		if (cur == -1)
			return -1;

		result += cur;
	}

	return result;
}

int main() {
	char buf[4096];

	while (1) {
		ssize_t bytes = read_(STDIN_FILENO, buf, sizeof(buf));

		if (bytes == 0 || bytes == -1)
			break;

		bytes = write_(STDOUT_FILENO, buf, bytes);
		
		if (bytes == -1)
			break;
	}

	return 0;
}
