#include <stdio.h>
#include <string.h>

/*
[Practice] 12 to 24 Time Conversion

Get input time in 12-hour clock and convert it into 24-hour clock.

Sample 1)
./run
Enter a time in 12-hour clock: 12:30 AM
Time converted in 24-hour clock: 00:30

Sample 2)
./run
Enter a time in 12-hour clock: 8:60 PM
Invalid Time!

Hint 1) You should use strcmp(str1, str2) and it returns 0 if they match. Note that '==' doesn't work to compare strings!
Hint 2) To print 1 => 01 and 11 => 11 (two digits), use '%02d' instead of '%d'.
*/


int main(void){
    
	/* Write your code here */

    char time[20];
    int h, m;
    char period[3];

    printf("Enter a time ( ex : 12:30 AM) : ");
    fgets(time,sizeof(time),stdin);

    // Parsing
    if(sscanf(time,"%2d:%2d %2s",&h,&m,period) != 3){
        printf("Wrong Type Error1\n");
        return 1;
    }

    if(h < 1 || h > 12 || m < 0 || m >= 60){
        printf("too high time etc. WRONG ERROR2\n");
        return 1;
    }

    // change to 24h mode

    if(strcmp(period,"AM")==0){
        if(h == 12){
            h = 00;
        }
    }else if(strcmp(period,"PM")==0){
        if(h < 12){
            h += 12;
        }
    }else{
        printf("AM,PM ERROR\n");
        return 1;
    }

    // print 24h mode
    printf("24h time : %02d:%02d %s\n",h,m,period);
//   _Bool a;
//   _Bool b;
//    a = (1==1);
//    b = (2==1);
//    printf("true: %d, false: %d ",a,b);  
	return 0 ;
}
