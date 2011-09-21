/*
  Original copyright header listed below. This comes via rsync.
  Any additional changes are provided via the same license as the original.

  Copyright (C) 2011 Muhammad Umair

*/
/*
 * Copyright (C) 1996-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <iostream>

namespace drizzled
{
namespace type
{

class IPv6 {

    struct ipv6_ds
    {
    	unsigned short ip6[8];
    } str;

    //Function to store the IPv4 address in IPv6 Data Structure
    int ipv4_inet_pton(const char *src)
    {
	char *ptr_src ;
	char ipv4_map_ipv6[20], ipv4[16], octet_1[5], octet_2[5], concat_octets[20];
	int octet[4], octet_index = 0 , dot_count = 0;

	static const char hex[] = "0123456789";
	const char * char_ptr_src;

	memset(ipv4_map_ipv6, 0, sizeof(ipv4_map_ipv6));
	strcpy(ipv4_map_ipv6, src);

	memset(octet, 0, sizeof(octet));
	memset(ipv4, 0, sizeof(ipv4));
	memset(octet_1, 0, sizeof(octet_1));
	memset(octet_2, 0, sizeof(octet_2));
	memset(concat_octets, 0, sizeof(concat_octets));

        ptr_src = ipv4_map_ipv6;

        if (*ptr_src == ':' && *++ptr_src != ':')
            return 0; // Invalid IP Address

        if (*ptr_src == ':' && *++ptr_src == ':')
            ++ptr_src;

	strcpy(ipv4,ptr_src);

	ptr_src = ipv4;

        while(*ptr_src != '\0')
        {
            dot_count++;

            if (*ptr_src == '.' && *++ptr_src != '.')
            {
                if (dot_count == 1)
                    return 0; // Invalid IP Address
            }

            char_ptr_src = strchr (hex, *ptr_src);

            if(char_ptr_src == NULL)
            {
                return 0; // Invalid IP Address
            }
            ++ptr_src;
        }

        ptr_src = ipv4;

	while(*ptr_src != '\0')
        {
            if ( *ptr_src == ':' && *++ptr_src == '\0')
            {
                    return 0; // Invalid IP Address
            }

            if ( *ptr_src == '.' && *++ptr_src == '\0')
            {
                    return 0; // Invalid IP Address
            }
          ++ptr_src;
        }

	ptr_src = strtok(ipv4,".");

	while (ptr_src != '\0')
	{
		sscanf(ptr_src, "%d", &octet[octet_index]);

		if(octet[octet_index++] > 255)
		{
			return 0; // Invalid IP Address
		}

		ptr_src = strtok (NULL, ".");
	}// end of main while loop

        if(octet[3] == 0)
        {
            octet[3] = octet[2];
            octet[2] = octet[1];
            octet[1] = octet[0];
            octet[0] = 0;
        }

	if(octet_index < 3 || octet_index > 4)
	{
		return 0; // Invalid IP Address
	}

	octet_index = 0;

        str.ip6[0] = str.ip6[1] = str.ip6[2] = str.ip6[3] = str.ip6[4] = str.ip6[5] = 0;

	for (int i=6 ; i <= 7; i++)
	{
		if (i == 7)
		{
			++octet_index;
		}

		sprintf(octet_1, "%02x", octet[octet_index]);

		sprintf(octet_2, "%02x", octet[++octet_index]);

		strcpy(concat_octets,octet_1);

		strcat(concat_octets,octet_2);

		sscanf(concat_octets,"%x",(unsigned int *)&str.ip6[i]);

		memset(octet_1, 0, sizeof(octet_1));

		memset(octet_2, 0, sizeof(octet_2));

		memset(concat_octets, 0, sizeof(concat_octets));
    	}

	return 1;
    }//end of ipv4_inet_pton() function

    //Function to retain the IPv4 address from IPv6 Data Structure
    char * ipv4_inet_ntop(char *destination)
    {
	memset(destination, 0, sizeof(destination));

	snprintf(destination, IPV6_BUFFER_LENGTH, "%03x:%03x:%03x:%03x:%03x:%03x:%03d.%03d.%03d.%03d" ,
	str.ip6[0],str.ip6[1],str.ip6[2],str.ip6[3],str.ip6[4],str.ip6[5],
	(((unsigned int )str.ip6[6]>>8) & 0xFF),
	((unsigned int )str.ip6[6] & 0xFF),
	(((unsigned int )str.ip6[7]>>8) & 0xFF),
	((unsigned int )str.ip6[7] & 0xFF));

	return destination;

    }// end of ipv4_inet_ntop function


    //Function to store the IPv6 address in IPv6 Data Structure
    int ipv6_inet_pton(const char *src)
    {
        if (strlen(src)> IPV6_DISPLAY_LENGTH)
        {
          return 0;   //Invalid IPaddress
        }

        //Local variables
        char ipv6[IPV6_BUFFER_LENGTH];

        memset(ipv6, 0, IPV6_BUFFER_LENGTH);

        strcpy(ipv6,src);

        char ipv6_temp[IPV6_BUFFER_LENGTH], ipv6_temp1[IPV6_BUFFER_LENGTH], ipv6_temp2[IPV6_BUFFER_LENGTH];

        memset(ipv6_temp, 0, IPV6_BUFFER_LENGTH);

        strcpy(ipv6_temp, ipv6);

        memset(ipv6_temp1, 0, IPV6_BUFFER_LENGTH);

        strcpy(ipv6_temp1, ipv6);

        memset(ipv6_temp2, 0, IPV6_BUFFER_LENGTH);

        strcpy(ipv6_temp2,ipv6);


        static const char hex[] = "0123456789abcdef";
        char temp[IPV6_BUFFER_LENGTH];
        char *ptr_src ,*ptr_char, *ptr_src1;
        const char *char_ptr_src;  // get the src char
        int char_int = 0, index_ip6 = 0, octet_count = 0, not_colon = 0, colon = 0, count_colon = 0;
        char temp_first[IPV6_BUFFER_LENGTH],temp_end[IPV6_BUFFER_LENGTH];

        memset(temp_first, 0, IPV6_BUFFER_LENGTH);

        memset(temp_end, 0, IPV6_BUFFER_LENGTH);

        ptr_src = ipv6;
        //while loop check three consective colons
         while (*ptr_src != '\0')
         {

            if (*ptr_src == ':' && *++ptr_src == ':' && *++ptr_src == ':')
            {
                return 0; // Invalid IP Address
            }

          ++ptr_src;
          }

        ptr_src = ipv6;

        //Check first colon position
        while (*ptr_src != '\0')
        {
            count_colon++;

            if (*ptr_src == ':' && *++ptr_src != ':')
            {
                if (count_colon == 1)
                    return 0; // Invalid IP Address
            }

          ++ptr_src;
        }

        ptr_src = ipv6;

        //Check last colon position
        while (*ptr_src != '\0')
        {
            if ( *ptr_src == ':' && *++ptr_src == '\0')
            {
                    return 0; // Invalid IP Address
            }

            ++ptr_src;
        }

        count_colon = 0;

        //while loop count the total number of octets
        ptr_src = strtok (ipv6_temp2,":");


        while (ptr_src != NULL)
        {
            octet_count++;

            ptr_src = strtok (NULL, ":");
        }
        //Retrun zero if total number of octets are greater than 8
        if(octet_count > 8)
        {
            return 0 ; // Invalid IP Address
        }

       bool colon_flag = true;
       ptr_src = ipv6;

	//Check the existance of consective two colons
        while (*ptr_src != '\0')
        {
            if (*ptr_src == ':' && *++ptr_src == ':')
            {
                colon_flag = false;
            }
            ++ptr_src;
        }

        if (colon_flag ==  true && octet_count < 8)
        {
            return 0;
        }

        int num_miss_octet =0;

	num_miss_octet = 8 - octet_count;
#if 0
        size = 2*num_miss_octet +1;
#endif

        std::string zero_append;

        ptr_src = ipv6_temp;

        //while loop locate the "::" position (start,middle or end)
        while(*ptr_src != '\0')
        {
            if(*ptr_src == ':' && *++ptr_src == ':')
            {
                if (*++ptr_src=='\0')
                {
                    colon =2;
                }
                else if (not_colon == 0)
                {
                    colon=1;
                }
                else
                {
                    colon=3;
                }

                count_colon++;

                if(count_colon == 2)
                {
                    return 0; // Invalid IP Address. Ther must be single time double  colon '::'
                }
            }

            ptr_src++;
            not_colon++;
        }// end of while loop

        // if colon = 0 means the IPv6 Address string is in presffered form otherwise first it covert it into prefeered form
        if(colon>0)
        {
            //zero padding format according to the '::' position
            zero_append+= "";

            for (int i= 0; i < num_miss_octet; i++)
            {
                if(colon==1) // start
                {
                    zero_append+= "0:";
                }

                if(colon==2 || colon==3)  //middle or end colon =2 shows at end
                {
                    zero_append+= ":0";
                }
            }

            if(colon==3)
            {
                zero_append+= ":";
            }

            ptr_src = temp_end;

            if(colon==1 || colon==3)
            {  //only for start and middle

              ptr_src1 = strstr (ipv6_temp,"::");

              ptr_src1 = ptr_src1+2;

              while(*ptr_src1 != '\0')
              {
                *ptr_src++ = *ptr_src1++;

                if(*ptr_src1 == '\0')
                {
                    *ptr_src ='\0';
                }
              }
            }

            //copy the input IPv6 string before and after '::'
            ptr_src1 = strstr (ipv6_temp1,"::");

            *ptr_src1 ='\0';

            strcpy(temp_first,ipv6_temp1);

            if(colon==2) // end
            {
                strcat(temp_first, zero_append.c_str());
            }
            else
            {
                strcat(temp_first, zero_append.c_str());

                strcat(temp_first,temp_end);
            }

            memset(ipv6, 0, IPV6_BUFFER_LENGTH);

            strcpy(ipv6,temp_first);
        }// end of main if statement



        //while loop store each octet on ipv6 struture in decimal value of hexadecimal digits
        ptr_char = temp;

        ptr_src = strtok (ipv6,":");


        while (ptr_src != NULL)
        {
                strcpy(temp, ptr_src);

                ptr_char = temp;

                int octet_length = strlen(ptr_char);

                *(ptr_char + octet_length) = '\0';


                while(*ptr_char != '\0')
                {
                    char_int = tolower(*ptr_char);

                    char_ptr_src = strchr (hex, char_int);

                    if(char_ptr_src == NULL)
                    {
                        return 0; // Invalid IP Address

                    }

                        *ptr_char = *char_ptr_src;
                        ptr_char++;
                }//end of inner while loop

                ptr_char -= octet_length;

                unsigned int ptr_char_val = 0;

                sscanf(ptr_char, "%x", (unsigned int *)&ptr_char_val);

                //check if "xxxx" value greater than "ffff=65535"
                if (ptr_char_val > 65535)
                {
                    str.ip6[0] = str.ip6[1] = str.ip6[2] = str.ip6[3] = str.ip6[4] = str.ip6[5] = str.ip6[6] = str.ip6[7] = 0;
                    return 0;
                }

                unsigned int *ptr = (unsigned int *)&(str.ip6[index_ip6++]);

                sscanf(ptr_char, "%x", ptr);

                memset(temp, 0, IPV6_BUFFER_LENGTH);

                ptr_src = strtok (NULL, ":");
        }// end of main while loop

        return 1;
    }// end of Ipv6_Inet_pton function

    //Function to retain the IPv6 address from IPv6 Data Structure
    char* ipv6_inet_ntop(char *destination)
    {
        char temp[10];

        memset(temp, 0, sizeof(temp));

        memset(destination, 0, IPV6_BUFFER_LENGTH);


        for (int i= 0; i <= 7; i++)
        {
            if(i==7)
            {
                sprintf(temp,"%04x",str.ip6[i]);

                strcat(destination,temp);
            }
            else
            {
                sprintf(temp,"%04x:",str.ip6[i]);

                strcat(destination,temp);
            }

            memset(temp, 0, sizeof(temp));
        }


        return destination;
    }// end of Ipv6_Inet_ntop function


    public:

    IPv6()
    {
        str.ip6[0] = str.ip6[1] = str.ip6[2] = str.ip6[3] = str.ip6[4] = str.ip6[5] = str.ip6[6] = str.ip6[7] = 0;
    }


    void store_object(unsigned char *out)
    {
        memcpy(out, (unsigned char *)&str, sizeof(str));
    }

    void restore_object(const unsigned char * in)
    {
    	memcpy(&str, (struct ipv6_ds *)in,  sizeof(str));
    }

    int inet_pton(const char *ip)
    {
         char * pch;

         pch=strchr((char *)ip,'.');

         if(pch == NULL)
         {
            return ipv6_inet_pton(ip);
         }
         else
         {
            return ipv4_inet_pton(ip);
         }
    }

    char * inet_ntop(char *dest)
    {
        if (str.ip6[0]==0 && str.ip6[1]==0 && str.ip6[2]==0 && str.ip6[3]==0 && str.ip6[4]==0 && str.ip6[5]==0 && str.ip6[6]!=0)
        {
            return ipv4_inet_ntop(dest);
        }
       else
        {
            return ipv6_inet_ntop(dest);
        }
    }

    static const size_t LENGTH= 16;
    static const size_t IPV6_DISPLAY_LENGTH= 39;
    static const size_t IPV6_BUFFER_LENGTH= IPV6_DISPLAY_LENGTH+1;

};  // endof class

} /* namespace type */
} /* namespace drizzled */

