/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                             *
 * Author: Jonathan Pennington                                 *
 * CS 4430: Database Management Systems                        *
 *                                                             *
 * Final Project: MP3 database and music player                *
 *                                                             *
 * Requires MYSQL, MYSQL C libraries,                          *
 * and SoX music player with MP3 plugin                        *
 *                                                             *
 * Written in C                                                *
 *                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <mysql/mysql.h>
#include <mysql/my_global.h>
#include <signal.h>

const int MAX_SIZE=255;

void handlePlay(MYSQL *, MYSQL_RES *, char *, char *);
void loadDatabase(MYSQL *, MYSQL_RES *, char *);
void printSongs(MYSQL *, MYSQL_RES *);
char ** tokenizeSong(char *, char **);

//*************************************************************************************

// This ignores SIGINT for user interface, but not during music output
static void handle_sigint(int signum) {}

//*************************************************************************************

int main (int argc, char ** argv){

	int i=0;
	char isConnected=(char)0;

	MYSQL * con = mysql_init(NULL);
	MYSQL_RES * results=NULL;

	char * username=malloc(MAX_SIZE);
	char * password=malloc(MAX_SIZE);

	char * option[3];
	option[0]=malloc(MAX_SIZE);
	option[1]=malloc(MAX_SIZE);
	option[2]=malloc(MAX_SIZE);

	// Used for keeping track of our option pointer
	char * originalopt[3];
	originalopt[0]=option[0];
	originalopt[1]=option[1];
	originalopt[2]=option[2];

	memset(option[0],0,MAX_SIZE);

	// Print Welcome Message
	printf("\nWelcome to the music application. Attempting connection to database");
	fflush(stdout);	// To make sure message prints

	// Do a fancy dot thingy for no practical reason
	for(i=0;i<3;i++){
		usleep(400000);
		printf(".");
		fflush(stdout);
	}
	usleep(400000);
	printf("\n");

	// Loop until connected
	printf("\ntype 'exit' for username if quitting\n\n");
	while(isConnected==(char)0){
		// Get the username for MYSQL localhost
		printf("Enter Username: ");
		if((fgets(username,MAX_SIZE,stdin)==NULL)) perror("Error reading username");
		if(strlen(username)==0) strcpy(username," ");
		username[strlen(username)-1]='\0';	//Strip the newline off
		if(strcmp(username,"exit")==0) {
			free(username);
			free(password);
			free(originalopt[0]);
			free(originalopt[1]);
			free(originalopt[2]);
			free(results);
			mysql_close(con);
			mysql_library_end();
			return 0;
		}
		// Get the password for localhost
		if((strcpy(password,getpass("Enter Password: "))==NULL)) perror("Error reading password");

		// Attempt MYSQL connection
		if((mysql_real_connect(con, "localhost", username, password ,NULL,0,NULL,0)==NULL)) {
			printf("Failed to connect with username and password. Please try again\n");
		}
		else isConnected=(char)1;
	}

	// Choose the music database. Create it if it doesn't exist
	if(mysql_query(con,"use music")!=0) {
		printf("Creating database\n");
		if(mysql_query(con,"CREATE DATABASE music")!=0) printf("Error creating database\n");
		if(mysql_query(con,"use music")!=0) printf("Error creating database\n");
		// Create the 'files' table
		if(mysql_query(con,"CREATE TABLE `files` ( `artist` varchar(255) NOT NULL DEFAULT '', `album` varchar(255) NOT NULL DEFAULT '', `trackNumber` int(3) DEFAULT NULL, `songName` varchar(255) NOT NULL DEFAULT '', `fileDirectory` varchar(255) NOT NULL DEFAULT '', `fileName` varchar(255) NOT NULL DEFAULT '', PRIMARY KEY (fileName, fileDirectory));")!=0) printf("unable to create file table\n");

	}

	printf("Successfully connected to the MYSQL database.\n");

	// Disabling SIGINT now
	signal(SIGINT,handle_sigint);

	while(strcmp(option[0],"exit\n")!=0){

		printf("Please type an option. Use the \"load\" option if new songs are added. Available options are: \n\nload\nplay song <song title>\nplay album <album title>\nplay artist <artist name>\nprint songs\ndelete database\n\nMy Option: ");

		// Reset the value of the input options
		option[0]=originalopt[0];
		option[1]=originalopt[1];
		option[2]=originalopt[2];

		memset(option[0],0,MAX_SIZE);
		memset(option[1],0,MAX_SIZE);

		// Get input for command
		if((fgets(option[0],MAX_SIZE,stdin)==NULL)) {
			printf("Thank you for pushing control-d. Exiting\n");
			free(username);
			free(password);
			free(originalopt[0]);
			free(originalopt[1]);
			free(originalopt[2]);
			free(results);
			mysql_close(con);
			mysql_library_end();
			return 0;
		}
		// Now we have to handle each of the options, separately with a method for each
		printf("\n");
		// First we tokenize the command
		option[0]=strtok_r(option[0]," ",&option[1]);
		option[1]=strtok_r(option[1]," ",&option[2]);
		option[2]=strtok(option[2],"\n");

		// Then we check for the argument
		if(strlen(option[0])==0) {	// To prevent segfault on empty entry
			strcpy(option[0]," ");
		}
		if(strcmp(option[0],"play")==0){
			handlePlay(con,results,option[1],option[2]);
		}
		// If the user types "load"
		else if(strcmp(option[0],"load\n")==0){
			printf("\nLoading music files into database\n\n");
			// Load the database
			loadDatabase(con,results,get_current_dir_name());
		}
		else if(strcmp(option[0],"print")==0){
			printSongs(con,results);
		}
		else if(strcmp(option[0],"delete")==0){
			// Delete current database
			if(mysql_query(con,"DROP DATABASE music")!=0) printf("Error deleting database");
			// Create an empy identical one
			if(mysql_query(con,"CREATE DATABASE music")!=0) printf("Error creating database\n");
			if(mysql_query(con,"use music")!=0) printf("Error creating database\n");
			// Create the 'files' table
			if(mysql_query(con,"CREATE TABLE `files` ( `artist` varchar(255) NOT NULL DEFAULT '', `album` varchar(255) NOT NULL DEFAULT '', `trackNumber` int(30) DEFAULT NULL, `songName` varchar(255) NOT NULL DEFAULT '', `fileDirectory` varchar(255) NOT NULL DEFAULT '', `fileName` varchar(255) NOT NULL DEFAULT '', PRIMARY KEY (fileName, fileDirectory));")!=0) printf("unable to create file table\n");
			printf("\nDatabase emptied\n\n");
		}
	}

	printf("Good Bye\n");
	mysql_close(con);
	mysql_library_end();
	free(username);
	free(password);
	free(originalopt[0]);
	free(originalopt[1]);
	free(originalopt[2]);
	free(results);

	return 0;
}

//*************************************************************************************

void handlePlay(MYSQL * con, MYSQL_RES * results, char * parameter1, char * parameter2){

	char * sqlQuery=malloc(MAX_SIZE);
	char * playArgs[MAX_SIZE];

	pid_t pid;
	int forkStatus=0;
	int i=0;

	MYSQL_ROW currentRow;

	// This is how we play albums
	if(strcmp(parameter1,"album")==0){
		if(snprintf(sqlQuery,MAX_SIZE,"SELECT fileName, fileDirectory FROM files WHERE album='%s' ORDER BY trackNumber",parameter2)<0)
			printf("Unable to correctly write SQL query\n");
		if((mysql_query(con,sqlQuery))!=0) {
			printf("Cannot find album\n");
		}
		if((results=mysql_store_result(con))==NULL) printf("Error storing data\n");
		playArgs[i]=malloc(MAX_SIZE);
		strcpy(playArgs[i++],"play");

		if((currentRow=mysql_fetch_row(results))==NULL){
			printf("Album not found in database\n\n");
			free(sqlQuery);
			for(;i<=0;i--) free(playArgs[i]);
			return;
		}
		else{
			playArgs[i]=malloc(MAX_SIZE);
			strcpy(playArgs[i],currentRow[0] ? currentRow[0] : "NULL");
			printf("%s\n",playArgs[i++]);
		}

		while((currentRow=mysql_fetch_row(results))){
			playArgs[i]=malloc(MAX_SIZE);
			strcpy(playArgs[i],currentRow[0] ? currentRow[0] : "NULL");
			printf("%s\n",playArgs[i++]);
		}
		playArgs[i]=malloc(MAX_SIZE);
		playArgs[i]='\0';

		if((pid=fork())==-1) printf("Fork error\n");
		if(pid==0){
			execvp(playArgs[0], playArgs);
		}
		else if(pid>0){
			waitpid(-1,&forkStatus,0);
		}
	}
	// This is how we play single songs
	else if(strcmp(parameter1,"song")==0){
		if(snprintf(sqlQuery,MAX_SIZE,"SELECT fileName, fileDirectory FROM files WHERE songName='%s'",parameter2)<0)
			printf("Unable to correctly write SQL query\n");
		if((mysql_query(con,sqlQuery))!=0){
			printf("Cannot find song\n");
			free(sqlQuery);
			for(;i<=0;i--) free(playArgs[i]);
			return;
		}
		if((results=mysql_store_result(con))==NULL) printf("Error storing data\n");
		char * playArgs[255];
		playArgs[i]=malloc(MAX_SIZE);
		strcpy(playArgs[i++],"play");
		printf("\n");
                if((currentRow=mysql_fetch_row(results))==NULL){
                        printf("Song not found in database\n\n");
			free(sqlQuery);
			for(;i<=0;i--) free(playArgs[i]);
                        return;
                }
                else{
                        playArgs[i]=malloc(MAX_SIZE);
                        strcpy(playArgs[i],currentRow[0] ? currentRow[0] : "NULL");
                        printf("%s\n",playArgs[i++]);
                }
		while((currentRow=mysql_fetch_row(results))){
			playArgs[i]=malloc(MAX_SIZE);
			strcpy(playArgs[i],currentRow[0] ? currentRow[0] : "NULL");
			printf("%s\n",playArgs[i++]);
			fflush(stdout);
		}
		playArgs[i]=malloc(MAX_SIZE);
		playArgs[i]='\0';

		// Now we fork so that we can run the play command
		if((pid=fork())==-1) printf("Fork error\n");
		if(pid==0){
			execvp(playArgs[0], playArgs);
		}
		else if(pid>0){
			waitpid(-1,&forkStatus,0);
		}
	}
	else if(strcmp(parameter1,"artist")==0){
		if(snprintf(sqlQuery,MAX_SIZE,"SELECT fileName, fileDirectory FROM files WHERE artist='%s' ORDER BY album, trackNumber",parameter2)<0)
			printf("Unable to correctly write SQL query\n");
		if((mysql_query(con,sqlQuery))!=0) {
			printf("Cannot find album\n");

		}
		if((results=mysql_store_result(con))==NULL) printf("Error storing data\n");
		char * playArgs[255];
		playArgs[i]=malloc(MAX_SIZE);
		strcpy(playArgs[i++],"play");
                if((currentRow=mysql_fetch_row(results))==NULL){
                        printf("Artist not found in database\n\n");
			free(sqlQuery);
			for(;i<=0;i--) free(playArgs[i]);
                        return;
                }
                else{
                        playArgs[i]=malloc(MAX_SIZE);
                        strcpy(playArgs[i],currentRow[0] ? currentRow[0] : "NULL");
                        printf("%s\n",playArgs[i++]);
                }
		while((currentRow=mysql_fetch_row(results))){
			playArgs[i]=malloc(MAX_SIZE);

			strcpy(playArgs[i],currentRow[0] ? currentRow[0] : "NULL");

			printf("%s\n",playArgs[i]);
			i++;

		}
		playArgs[i]=malloc(MAX_SIZE);
		playArgs[i]='\0';

		if((pid=fork())==-1) printf("Fork error\n");
		if(pid==0){
			execvp(playArgs[0], playArgs);
		}
		else if(pid>0){
			waitpid(-1,&forkStatus,0);
		}
	}
	free(sqlQuery);
	for(;i<=0;i--) free(playArgs[i]);
	return;
}

//*************************************************************************************

void loadDatabase(MYSQL * con, MYSQL_RES * results, char * location){

	// Learned about dp and dirent in CS-2240
	DIR * dp=NULL;
	struct dirent *dirp;
	char ** songInfo=malloc(MAX_SIZE);
	int i=0;
//	songInfo[0]=malloc(MAX_SIZE);
	for(i=0;i<5;i++){
		songInfo[i]=malloc(MAX_SIZE);
	}
	char * fileName=malloc(MAX_SIZE);
	char * fullFilePath=malloc(4*MAX_SIZE);

	char * mysqlQuery=malloc(4*MAX_SIZE);

	memset(fullFilePath,0,4*MAX_SIZE);
	memset(mysqlQuery,0,(4*MAX_SIZE));

	// First we open the directory of the program
	if(chdir(location)<0){
		perror("Cannot find directory");
		free(songInfo);
		free(fileName);
		free(fullFilePath);
		free(mysqlQuery);
		return;
	}


	if((dp=opendir("."))==NULL) perror("Cannot open directory");

	// Read the directory until it's empty
	while((dirp=readdir(dp))!=NULL){
		// Check to see whether the filename is larger than 4 characters to prevent segfault
		if(strlen(dirp->d_name)>=4){
			strcpy(fileName,dirp->d_name);	// Copy full filename to fileName
			// Check to see whether it's an MP3 file (by checking extension)
			if(strcmp(&fileName[strlen(fileName)-4],".mp3")==0){
				// Gather and store information about the file
				printf("%s is an mp3 file\n",fileName);
				// Create full file path
				memset(fullFilePath,0,(4*MAX_SIZE));
				snprintf(fullFilePath,4*MAX_SIZE,"%s/\"%s\"",location,fileName);

				songInfo=tokenizeSong(fileName,songInfo);

				// Create mysql query statement
				snprintf(mysqlQuery,4*MAX_SIZE,"INSERT INTO files VALUES ('%s','%s',%s,'%s','%s','%s')",songInfo[0],songInfo[1],songInfo[2],songInfo[3],location,fileName);

				printf("\n\n");

				// Add the file to the database
				mysql_query(con,mysqlQuery);
			}
		}
	}
	free(songInfo);
	free(fileName);
	free(fullFilePath);
	free(mysqlQuery);
	return;
}

//*************************************************************************************

void printSongs(MYSQL * con, MYSQL_RES * results){

	MYSQL_ROW currentRow;

	if(mysql_query(con,"SELECT artist, trackNumber, album, songName FROM files ORDER BY artist,album,trackNumber")!=0) printf("Query failed on method printSongs\n");
	if((results=mysql_store_result(con))==NULL) printf("Failed to store results in method printSongs\n");
	printf("%-23s %-14s%-30s %-30s\n","Artist","Track Number","Album","Song Title");
	printf("-------------------------------------------------------------------------------------------------\n");
	while((currentRow=mysql_fetch_row(results))) printf("%-23s %2d            %-30s %-30s\n",currentRow[0],atoi(currentRow[1]),currentRow[2],currentRow[3]);

}

//*************************************************************************************

char **  tokenizeSong(char * fileName,char ** songInfo){
	char * tempForTok=malloc(MAX_SIZE);
	char * tokAddr=tempForTok;
	strcpy(tempForTok,fileName);	// Make a copy so the fileName doesn't have nulls from tokenizing

	// What we need: Artist name, Album Name, Track Number, Song Name, Path to file
	strcpy(songInfo[0],strtok_r(tempForTok,"-",&tempForTok));
	strcpy(songInfo[1],strtok_r(tempForTok,"-",&tempForTok));
	strcpy(songInfo[2],strtok_r(tempForTok,"-",&tempForTok));
	tempForTok[strlen(tempForTok)-4]='\0';
	strcpy(songInfo[3],tempForTok);


	// Add the file to the database
	printf("Artist: %s\n",songInfo[0]);
	printf("Album:  %s\n",songInfo[1]);
	printf("Track:  %s\n",songInfo[2]);
	printf("Song:   %s\n",songInfo[3]);

	free(tokAddr);

	return songInfo;

}

//*************************************************************************************
