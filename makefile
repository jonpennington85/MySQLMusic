FILENAME = mysqlmusic.c
OUTPUT = mysqlmusic

main: ${FILENAME}
	gcc -Wall -Werror ${FILENAME} -o ${OUTPUT} `mysql_config --cflags --libs`

make clean:
	rm -vv ${OUTPUT}
