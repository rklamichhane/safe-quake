#include <inttypes.h>
#include<string.h>
#include <avr/io.h>
#include<stdio.h>
#include<stdlib.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include<util/delay.h>
#define x_ul 165
#define x_ll 155
#define y_ul 167
#define y_ll 155
#define z_ll 198
#define z_ul 205
#define data PORTD

#define ctrl PORTB
#define en 0
#define rs 1
#define buff_size 64
#define SIM_OK 1
#define SIM_DOK 111
#define SIM_TIMEOUT -3
#define SIM_INVALID_RESPONSE -1
#define SIM_FAIL -2
#define REGISTERED_HOME 5
#define NETWORK_SEARCHING 9
#define NETWORK_REGISTED_ROAMING 1
#define NETWORK_ERROR 99
#define SIM_NOT_READY 100
#define MSG_EMPTY 101
unsigned int key;
unsigned char value[]={'0','1','2','3','4','5','6','7','8','9'};
//unsigned char week[][4]={"hell","oooo","SUN\0","MON\0","TUE\0","WED\0","THU\0","FRI\0","SAT\0"};
volatile char ubuff[buff_size];
char sim_buff[128];
 static volatile char gg=0;

volatile int time=0;
volatile int8_t head;
volatile int8_t tail;
void lcd_cmd(unsigned char cmd)
{
   PORTB&=~(1<<rs);
   data=cmd&0xf0;
   ctrl|=(1<<en);
   _delay_us(1000);
   ctrl&=~(1<<en);
   _delay_us(10000);
   data=cmd<<4;
   ctrl|=(1<<en);
   _delay_us(1000);
   ctrl&=~(1<<en);
   _delay_us(1000);
}
void lcd_data(unsigned char dta)
{
   ctrl|=(1<<rs);
   data=dta&0xf0;
   ctrl|=(1<<en);
   _delay_us(1000);
   ctrl&=~(1<<en);
   _delay_us(1000);
   data=dta<<4;
   ctrl|=(1<<en);
   _delay_us(1000);
   ctrl&=~(1<<en);
   _delay_us(1000);
}
void lcd_init()
{
   DDRD=0xff;
   DDRB=0xff;
   ctrl&=~(1<<en);
   _delay_us(1000);
   lcd_cmd(0x33);
   lcd_cmd(0x32);
   lcd_cmd(0x28);
   lcd_cmd(0x0e);
   _delay_us(2000);
   lcd_cmd(0x06);
   _delay_us(1000);
}
void lcd_gotoxy(unsigned char x, unsigned char y)
{
   unsigned char add[]={0x80,0xC0};
   lcd_cmd(add[x-1]+y-1);
   _delay_us(1000);
}
void lcd_strng( char *str)
{
   unsigned char i=0;
   while(str[i]!=0)
   {
      lcd_data(str[i]);
      i++;
   }
}
ISR(USART_RXC_vect)
{
   char data1=UDR;
   if(((tail==buff_size-1)&&head==0)||((tail+1)==head))
   {
      //circular array full vayo kinaki head paxadi thyakkai tail xa
      head++;
      if(head==buff_size)
	 head=0;
   }
   if(tail==(buff_size-1))
      tail=0;
   else tail++;
   ubuff[tail]=data1;
   if(head==-1)
      head=0;
}
char read()
{
   char data1;
   if(head==-1)
      return 0;
   data1=ubuff[head];
   if(head==tail)
   {
      head=tail=-1;
   }
   else
   {
      head++;
      if(head==buff_size)
	 head=0;
   }
   return data1;
}
uint8_t data_amount()
{
	if(head==-1) return 0;
	if(head<tail)
		return(tail-head+1);
	else if(head>tail)
		return (buff_size-head+tail+1);
	else
		return 1;
}
void lcd_cmd1()
{
   lcd_cmd(0x01);
   _delay_ms(100);
}
void send(unsigned char ch)
{
   while(!(UCSRA&(1<<UDRE)));
      UDR=ch;
}
void send_strng( char *str)
{
   while((*str)!='\0')
   {
      send(*str);
      str++;
   }
}
void read_strng(void *buffr,uint16_t len)
{
	uint16_t i;
	for(i=0;i<len;i++)
	{
		((char*)buffr)[i]=read();
	}
}
void flush()
{
	while(data_amount()>0)
	{
		read();
	}
}

   
int8_t sim_cmd(const char *cmnd)
{
   send_strng(cmnd);
   send(0x0D);
   unsigned char i=0;
   uint8_t len=strlen(cmnd);
   len++;
   while(i<10*len)
	{
		if(data_amount()<len)
		{
			i++;
			
			_delay_ms(10);
			
			continue;
		}
		else
		{
			//We got an echo
			//Now check it
			read_strng(sim_buff,len);	//Read serial Data
			
			return SIM_OK;
			
		}
	}
		return SIM_TIMEOUT;
			
}
int8_t check_r(const char *response,const char *check,uint8_t len)
{
	len-=2;
	
	//Check leading CR LF
	if((response[0]!=0x0D )| (response[1]!=0x0A))
		return	SIM_INVALID_RESPONSE;
	
	//Check trailing CR LF
	if((response[len]!=0x0D )| (response[len+1]!=0x0A))
		return	SIM_INVALID_RESPONSE;
		
	
	//Compare the response
	uint8_t i;
	for(i=2;i<len;i++)
	{
		if(response[i]!=check[i-2])
			return SIM_FAIL;
	}
	
	return SIM_OK;		
}
int8_t wait(uint16_t timeout)
{
	uint8_t i=0;
	uint16_t n=0;
	
	while(1)
	{
		while (data_amount()==0 && n<timeout){n++; _delay_ms(1);}
	
		if(n==timeout)
			return 0;
		else
		{
			sim_buff[i]=read();
		
			if(sim_buff[i]==0x0D && i!=0)
			{
				flush();
				return i+1;
			}				
			else
				i++;
		}
	}	
}
int8_t sim_init()
{
	
	//Check communication line
	sim_cmd("AT");	//Test command
	
	//Now wait for response
	uint16_t i=0;
	while(i<10)
	{
		if(data_amount()<6)
		{
			i++;
			
			_delay_ms(10);
			
			continue;
		}			
		else
		{
			//We got a response that is 6 bytes long
			//Now check it	
			read_strng(sim_buff,6);	//Read serial Data
			
			return check_r(sim_buff,"OK",6);
				
		}			
	}

	return SIM_TIMEOUT;
			
}
int8_t network_status()

{
	sim_cmd("AT+CREG?");
	uint16_t i=0;
	
	//correct response is 20 byte long
	//So wait until we have got 20 bytes
	//in buffer.
	while(i<10)
	{
		if(data_amount()<20)
		{
			i++;
			
			_delay_ms(10);
			
			continue;
		}
		else
		{
			//We got a response that is 20 bytes long
			//Now check it
			read_strng(sim_buff,20);	//Read serial Data
			
			if(sim_buff[11]=='1')
				return REGISTERED_HOME;
			else if(sim_buff[11]=='2')
				return NETWORK_SEARCHING;
			else if(sim_buff[11]=='5')
				return NETWORK_REGISTED_ROAMING;
			else
				return NETWORK_ERROR;			
		}
	}
	
	return SIM_TIMEOUT;
	
}

int8_t	send_msz(const char *num, const char *msg,int8_t *msg_ref)
{
	flush();
	
	char cmd[25];
	
	sprintf(cmd,"AT+CMGS= %s",num);
	
	cmd[8]=0x22; //"
	
	uint8_t n=strlen(cmd);
	
	cmd[n]=0x22; //"
	cmd[n+1]='\0';
	
	//Send Command
	sim_cmd(cmd);
	
	_delay_ms(100);
	
	send_strng(msg);
	
	send(0x1A);
	
	while(data_amount()<(strlen(msg)+5));
	
	//Remove Echo
	read_strng(sim_buff,strlen(msg)+5);
	
	uint8_t len=wait(3000);
	
	if(len==0)
		return SIM_TIMEOUT;
	
	sim_buff[len-1]='\0';
	
	if(strncasecmp(sim_buff+2,"CMGS:",5)==0)
	{
		
		*msg_ref=atoi(sim_buff+8);
		
		flush();
		
		return SIM_OK;
	}
	else
	{
		flush();
		return SIM_FAIL;	
	}		
}


   
   
   
   
   
   
uint8_t ntc_ki_ncell(char *name)
{
	flush();
	
	//Send Command
	sim_cmd("AT+CSPN?");
	
	uint8_t len=wait(1000);
	
	if(len==0)
		return SIM_TIMEOUT;
	
	char *start,*end;
	start=strchr(sim_buff,'"');
	start++;
	end=strchr(start,'"');
	
	*end='\0';
	
	strcpy(name,start);
	
	return strlen(name);
}
int8_t	dlt_msz(uint8_t i)
{
	flush();
	
	//String for storing the command to be sent
	char cmd[16];
	
	//Build command string
	sprintf(cmd,"AT+CMGD=%d",i);
	
	//Send Command
	sim_cmd(cmd);
	
	uint8_t len=wait(1000);
	
	if(len==0)
		return SIM_TIMEOUT;
	
	sim_buff[len-1]='\0';
	
	//Check if the response is OK
	if(strcasecmp(sim_buff+2,"OK")==0)
	{
	   lcd_cmd1();
	lcd_strng("msz dltd");
	_delay_ms(100);
		return SIM_OK;
	}
	else
	{
	   lcd_cmd1();
	   lcd_strng("dlting failed");
		return SIM_FAIL;
	}
}
/*int8_t wait_msz(uint8_t *id)
{	
	uint8_t len=wait(1250);
   //uint8_t len=data_amount();
	
	if(len==0)
		return SIM_TIMEOUT;
	
	sim_buff[len-1]='\0';
	
	 if(strncasecmp(sim_buff+2,"+CMTI:",6)==0)
	{
		char str_id[4];
		
		char *start;
		
		start=strchr(sim_buff,',');
		start++;
	
		strcpy(str_id,start);
		
		*id=atoi(str_id);
		
		return SIM_OK;
	}
     if(strncasecmp(sim_buff+2,"RING",4)==0)
	{
		sim_cmd("ATA");
		
		return SIM_DOK;
	}	
	else
		return SIM_FAIL;
}*/
int8_t	read_msz(uint8_t i, char *msg)
{
	//Clear pending data in queue
	flush();
	;
	//String for storing the command to be sent
	char cmd[16];
	
	//Build command string
	sprintf(cmd,"AT+CMGR=%d",i);
	
	//Send Command
	sim_cmd(cmd);
	
	uint8_t len=wait(1250);
	
	if(len==0)
	{
	   lcd_cmd1();
	lcd_strng("time out");
	   _delay_ms(100);
		return SIM_TIMEOUT;
	}
	
	sim_buff[len-1]='\0';
	
	//Check of SIM NOT Ready error
	if(strcasecmp(sim_buff+2,"+CMS ERROR: 517")==0)
	{
		lcd_cmd1();
	   lcd_strng("SIM_NOT_READY");
	   _delay_ms(100);
		return SIM_NOT_READY;
	}
	
	//MSG Slot Empty
	if(strcasecmp(sim_buff+2,"OK")==0)
	{
	   lcd_cmd1();
	   lcd_strng("MSG_EMPTY");
	   _delay_ms(100);
		return MSG_EMPTY;
	}
		
	//Now read the actual msg text
	len=wait(1000);
	
	if(len==0)
	{
	   lcd_cmd1();
	   lcd_strng("SIM TIME OUT");
	   _delay_ms(100);
		return SIM_TIMEOUT;
	}
	
	sim_buff[len-1]='\0';
	strcpy(msg,sim_buff+1);
	//len=wait(1000);
       // sim_buff[len-1]='\0';
	//strcpy(number,sim_buff+1);

	
	return SIM_OK;
}
int8_t wait_msz(uint8_t *id)
{	
	uint8_t len=wait(1250);
   //uint8_t len=data_amount();
	
	if(len==0)
		return SIM_TIMEOUT;
	
	sim_buff[len-1]='\0';
	
	if(strncasecmp(sim_buff+2,"+CMTI:",6)==0)
	{
		char str_id[4];
		
		char *start;
		
		start=strchr(sim_buff,',');
		start++;
	
		strcpy(str_id,start);
		
		*id=atoi(str_id);
		
		return SIM_OK;
	}		
	else
	{
		return SIM_FAIL;
	   lcd_cmd1();
	   lcd_strng("read vayena");
	   _delay_ms(1000);
	}
}

void usart_init()
{
   head=tail=-1;
   UCSRB=(1<<RXEN)|(1<<TXEN)|(1<<RXCIE);
   UCSRC=(1<<URSEL)|(3<<UCSZ0);
   UBRRL=103;
}
void all_init()
{
   int8_t temp;
   char DD[10];
   usart_init();
   lcd_init();
   lcd_cmd1();
   sei();
   temp=sim_init();
   lcd_cmd1();
   if(temp==SIM_OK)
   {
      lcd_strng("SIM OK");
   _delay_ms(1000);
   }
   lcd_cmd(0xc0);
   _delay_ms(10);
   temp=ntc_ki_ncell(DD);
   lcd_strng(DD);
   _delay_ms(1000);
}
void start_init()
{
   sim_cmd("AT+CPMS=\"SM\"");
   _delay_ms(1000);
   sim_cmd("AT+CMGF=1");
   _delay_ms(1000);
   sim_cmd("AT+CNMI=1,1,0,0,0");
   _delay_ms(1000);
   sim_cmd("AT+CSAS");
   _delay_ms(10000);
   _delay_ms(10000);
   _delay_ms(10000);
   _delay_ms(10000);
}
/*void send_status()
{
   char status[40],l1[3],l2[3],f1[3],f2[3];
   char num[]="9818813433";
   unsigned char k;
   if(SRL==1)
   strcpy(l1,"on");
   else
      strcpy(l1,"off");
   if(ML==1)
      strcpy(l2,"on");
   else
      strcpy(l2,"off");
   if(CRF==1)
      strcpy(f1,"on");
   else
      strcpy(f1,"off");
   if(ORF==1)
      strcpy(f2,"on");
   else
      strcpy(f2,"off");
   sprintf(status,"light 1=%s light 2=%s fan 1=%s fan 2=%s",l1,l2,f1,f2);
   send_msz("+9779818813433",status,&k);
}*/
void saftey()
{
   DDRB=0XFF;
   PORTB|=(1<<PB2)|(1<<PB4)|(1<<PB5);
}
void adc_init()
{
    ADMUX|=(1<<REFS0)|(1<<REFS1)|(1<<ADLAR)|(1<<MUX0);
   //ADMUX=0X87;
    ADCSRA|=(1<<ADEN)|(1<<ADPS0)|(ADPS1)|(1<<ADPS2);
 //ADCSRA=0XE0;
}
 uint8_t get_value(uint8_t ch)
{
 char temp;
   ch=(ch&0b00000111);
    ADMUX=(ADMUX&0XE0)|ch;
      ADCSRA|=(1<<ADSC);
    while((ADCSRA&(1<<ADIF))==0){};
    ADCSRA|=(1<<ADIF);
       temp=ADCH;
    //lcd_cmd1();
    //lcd_intg(temp,2);
    //_delay_ms(100);
    return temp;
 }
 void lcd_intg(int a,unsigned int b)
{
 
   int d1[5],i=0;
   for(i=0;i<b;i++)
   {
   d1[i]=a%10;
   a/=10;
   
   }
   for(i=b;i>=0;i--)
   lcd_data(value[d1[i]]);
}
void inline send_msz_all(unsigned char a,const char *msz)
{
   char k,number[3][15]={"+9779806504433","+9779806887654","+9779808648273"};
   
     if(a!=1)
     {
     send_msz(number[0],msz,&k);
     _delay_ms(1000);
	lcd_strng2("sent 1");
	_delay_ms(1000);
     }
     if(a!=2)
   {
     send_msz(number[1],msz,&k);
     _delay_ms(1000);
      lcd_strng2("sent 2");
      _delay_ms(1000);
   }
     if(a!=3)
     {
     send_msz(number[2],msz,&k);
     _delay_ms(1000);
	lcd_strng2("sent 3");
	_delay_ms(1000);
     }
   
}
void start_timer()
{
   TCNT1H=(-31250)>>8;
    TCNT1L=-31250;
   TCCR1A=0X00;
   TCCR1B=0x04;
   TIMSK=(1<<TOIE1);
   sei();
}
void lcd_strng2( char *str)
{
   unsigned char i=0;
   lcd_cmd1();
   while(str[i]!=0)
   {
      lcd_data(str[i]);
      i++;
      if(i==16)
	 lcd_cmd(0xc0);
      
   }
}


int main()
{
  char temp,strng[60];
   static char s1=0,r2=0,s3=0;
   char message[]="earthquake detected.....are you safe?? from safequake ";
//   char number[][14]={"+9779846887654","+9779806504433","+9779818813433"};
   uint8_t x,y,z;
   
   char cmd[30];
   uint8_t idd;
   
  all_init();
   //lcd_init();
   adc_init();
   sei();
  //start_init();
  sim_cmd("AT+CLIP=1");
   lcd_cmd1();
    lcd_strng("NORMAL");
   //lcd_cmd(0x01);
   _delay_ms(10);
   /*
    lcd_strng("   X    Y    Z");
   while(1)
   {
      x=get_value(1);
      _delay_ms(10);
      y=get_value(2);
      _delay_ms(10);
      z=get_value(3);
    
    lcd_gotoxy(2,2);
    lcd_intg(x,3);
    lcd_gotoxy(2,6);
    lcd_intg(y,3);
    lcd_gotoxy(2,12);
    lcd_intg(z,3);
   }
 */
 while(1)
 {
     x=get_value(1);
      _delay_ms(10);
      y=get_value(2);
      _delay_ms(10);
      z=get_value(3);
   if((x<=x_ll)||(x>=x_ul)||(y<=y_ll)||(y>=y_ul)||(z<=z_ll)||(z>=z_ul))
      {
	 lcd_cmd1();
	 lcd_strng("...EARTHQUAKE...");
	 lcd_cmd(0xc0);
	 lcd_strng("    DETECTED    ");
	 saftey();
	 send_msz_all(6,message);
	 //send_msz_all(6,message);
	start_timer();
	 while(1)
	 {
	    lcd_strng2("waiting for response");
	 if(wait_msz(&idd)==SIM_OK)
	 {
	 temp=read_msz(idd,strng);
	 lcd_strng(strng);
	 _delay_ms(90);
	    if(strcasecmp(strng,"ok1"))
	    {
	       sprintf(cmd,"%s has informed that he is ok","sonup");
	       s1=1;
	       send_msz_all(1,cmd);
	       lcd_strng2(cmd);
	    }
	    
	    else if(strcasecmp(strng,"ok2"))
	    {
	       sprintf(cmd,"%s has informed that he is ok","RK");
	       r2=1;
	       send_msz_all(2,cmd);
	       lcd_strng2(cmd);
	    }
	    else if(strcasecmp(strng,"ok3"))
	    {
	       sprintf(cmd,"%s has informed that he is ok","sovit");
	       s3=1;
	       send_msz_all(3,cmd);
	       lcd_strng2(cmd);
	    }
	       dlt_msz(idd);
	 }
	 
	 if(time>=300)
	    break;
      }


	   if(time>=300)
	   {
	   if(s1==0)
	   {
	      sprintf(cmd,"%s is in danger,help him imdtly","sonup");
	      send_msz_all(1,cmd);
	      lcd_strng2(cmd);
	   }
	   if(r2==0)
	   {
	      sprintf(cmd,"%s is in danger,help him imdtly","RK");
	      send_msz_all(2,cmd);
	   }
	   if(s3==0)
	   {
	      sprintf(cmd,"%s is in danger,help him imdtly","sovit");
	      send_msz_all(3,cmd);
	      lcd_strng2(cmd);
	   }
	   if((s1&&r2&&s3)!=0)
	   {
	      sprintf(cmd,"all are safe :) balle balle");
	      send_msz_all(4,cmd);
	      lcd_strng2(cmd);
	   }
	   break;
	      
	}
	
       dlt_msz(idd);
      
      }
   
}
   
  return 0;
}
ISR (TIMER1_OVF_vect)
{
   TCNT1H=(-31250)>>8;
    TCNT1L=(-31250)&0xff;
   time+=1;
   //lcd_strng("hii");
}
