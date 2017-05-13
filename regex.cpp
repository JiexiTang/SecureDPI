#include "stdafx.h"
#include "regex.h"

regex::regex(unsigned n_regex, int seed){
	num_regex = n_regex;
	long_tokens=NULL;
	short_tokens=NULL;
	num_long_tokens=0;
	num_short_tokens=0;
	srand(seed);
	
	//init parameters
    parameters.min_length			= CONFIG_REGEX_MIN_LENGTH;
    parameters.max_length			= CONFIG_REGEX_MAX_LENGTH ;
    parameters.avg_length			= CONFIG_REGEX_AVG_LENGTH ;
    parameters.min_sp_length		= CONFIG_SP_MIN_LENGTH ;
    parameters.max_sp_length		= CONFIG_SP_MAX_LENGTH ;
    parameters.avg_sp_length		= CONFIG_SP_AVG_LENGTH ;

    parameters.special_range[0]		= CONFIG_FREQ_S ;
    parameters.special_range[1]		= CONFIG_FREQ_W ;
    parameters.special_range[2]		= CONFIG_FREQ_D ;
    parameters.special_range[3]		= CONFIG_FREQ_NOT_S ;
    parameters.special_range[4]		= CONFIG_FREQ_NOT_D ;
    parameters.special_range[5]		= CONFIG_FREQ_NOT_W ;
    parameters.range				= CONFIG_FREQ_RANGE ;
    parameters.single_wildcard		= CONFIG_FREQ_WILDCARD ;
    parameters.simple_repetitions	= CONFIG_FREQ_SIMPLE_REP ;
    parameters.special_range_rep[0] = CONFIG_FREQ_S_REP ;
    parameters.special_range_rep[1] = CONFIG_FREQ_W_REP ;
    parameters.special_range_rep[2] = CONFIG_FREQ_D_REP ;
    parameters.special_range_rep[3] = CONFIG_FREQ_NOT_S_REP ;
    parameters.special_range_rep[4] = CONFIG_FREQ_NOT_D_REP ;
    parameters.special_range_rep[5] = CONFIG_FREQ_NOT_W_REP ;
    parameters.range_rep			= CONFIG_FREQ_RANGE_REP ;
    parameters.dot_star				= CONFIG_FREQ_DOT_STAR ;
    parameters.not_newline_star		= CONFIG_FREQ_NOTNEWLINE_STAR ;
    parameters.pattern_rep			= CONFIG_FREQ_PATTERN_REP ;
    parameters.simple_counting		= CONFIG_FREQ_SIMPLE_COUNTING ;
    parameters.range_counting		= CONFIG_FREQ_RANGE_COUNTING ;
    parameters.pattern_counting		= CONFIG_FREQ_PATTERN_COUNTING ;
    parameters.counting				= CONFIG_FREQ_COUNTING ;
    parameters.or_expr				= CONFIG_FREQ_OR_EXPR ;
}

regex::~regex(){
	if (long_tokens!=NULL){
		for (unsigned i=0;i<num_long_tokens;i++) free(long_tokens[i]);
		free(long_tokens);
	}
	if (short_tokens!=NULL){
		for (unsigned i=0;i<num_short_tokens;i++) free(short_tokens[i]);
		free(short_tokens);
	}
}	

void regex::read_tokens(FILE *file){

	unsigned i=0,j=0,k=0;
	unsigned num_tokens=0;
	num_short_tokens=0;
	num_long_tokens=0;
	
	char *token = allocate_char_array(1000);
	int r = fscanf_s(file,"%s\n",token,1000);
	//统计tokens数量
	while(r!=EOF && r>0){
		r=fscanf_s(file,"%s\n",token,1000);
		if (r!=EOF && r>0) num_tokens++;
	}
	free(token);
	
	//读出所有的token，并存放到tokens数组中
	char **tokens = allocate_string_array(num_tokens);
	rewind(file);
	while (i<num_tokens){
		char *token =(char *)malloc(1000);
		int a = fscanf_s(file,"%s\n",token,1000);
		tokens[i++]=token;
	}
	
	//统计短token数量，长token数量
	for (int i=0;i<num_tokens;i++){
		if (strlen(tokens[i])==1 ||( strlen(tokens[i]) <=3 && tokens[i][0]=='\\') ||
			( strlen(tokens[i]) <=4 && tokens[i][0]=='\\' && tokens[i][1]=='x'))
			num_short_tokens++;
	}
	num_long_tokens=num_tokens-num_short_tokens;
	
	short_tokens = allocate_string_array(num_short_tokens);
	long_tokens  = allocate_string_array(num_long_tokens);
	for (int i=0;i<num_tokens;i++){
		if (strlen(tokens[i])==1 ||( strlen(tokens[i]) <=3 && tokens[i][0]=='\\') ||
			( strlen(tokens[i]) <=4 && tokens[i][0]=='\\' && tokens[i][1]=='x'))
			short_tokens[j++]=tokens[i];	//找出所有短的token
		else
			long_tokens[k++]=tokens[i];		//找出所有长的token
	}	
	free(tokens);
}

//向上取整
inline int top(int a,int b) { if (a%b==0) return a/b; else return (a/b)+1;}

bool more_feature(unsigned features[24]){
	for (int i=0;i<23;i++) if (features[i]!=0) return true;
	return false;
}

//生成Regex
void regex::build_re(FILE *output_file){
	unsigned features[24];  
	for (int i=0;i<6;i++) features[i]=(unsigned)(parameters.special_range[i]*num_regex);
	features[6]  = (unsigned)(parameters.range*num_regex); 		    
	features[7]  = (unsigned)(parameters.single_wildcard*num_regex);  
	features[8]  = (unsigned)(parameters.simple_repetitions*num_regex);   
	for (int i=0;i<6;i++) features[i+9] = (unsigned)(parameters.special_range_rep[i]*num_regex); 
	features[15] = (unsigned)(parameters.range_rep*num_regex);
	features[16] = (unsigned)(parameters.dot_star*num_regex); 
	features[17] = (unsigned)(parameters.not_newline_star*num_regex); 
	features[18] = (unsigned)(parameters.pattern_rep*num_regex); 
	features[19] = (unsigned)(parameters.simple_counting*num_regex); 
	features[20] = (unsigned)(parameters.range_counting*num_regex);
	features[21] = (unsigned)(parameters.pattern_counting*num_regex);
	features[22] = (unsigned)(parameters.counting*num_regex);
	features[23] = (unsigned)(parameters.or_expr*num_regex);
	unsigned ft_per_re =0; //number of non-exact match sub-patterns per regex
	for (int i=0;i<23;i++) ft_per_re+=features[i]; //treat or-expr in a special way
	if (features[23]!=0) ft_per_re+=top(features[23],randint(1,top(features[23],2)));
	ft_per_re = top(ft_per_re,num_regex);
	
	/* regex generation */
	for (unsigned i=0;i<num_regex;i++){
		/* regular expression length
		   - in this setting we consider a random value uniformly distributed between min_length and max_length
		     with probability 1/4, and between min_length and min_length+max_length/2 with probability 3/4 
		*/
		unsigned length =0, target_length=0; 
		int sel=randint(1,4);
		if (sel!=4)
			target_length=randint(parameters.min_length,(parameters.min_length+parameters.max_length)/2); 
		else
			target_length=randint(parameters.min_length,parameters.max_length);
		
		//number of non-exact match sub-patterns in current regex
		unsigned num_features=ft_per_re;
		if (target_length > (parameters.min_length+parameters.max_length)/2) num_features++;
		
		//regex generation
		while(length<target_length){
			int lt=randint(0,num_long_tokens-1);
			fprintf(output_file,"%s",long_tokens[lt]);
			length+=strlen(long_tokens[lt]);
			int c0,c1;
			if (length>target_length) break;
			if (num_features!=0 && more_feature(features)){
				int f=randint(0,23);
				while (features[f]==0) f=randint(0,23);
				switch (f){
					case 0:
						fprintf(output_file,"\\s");
						length++;
						break;
					case 1:
						fprintf(output_file,"\\d");
						length++;
						break;
					case 2:
						fprintf(output_file,"\\w");
						length++;
						break;
					case 3:
						fprintf(output_file,"\\S");
						length++;
						break;
					case 4:
						fprintf(output_file,"\\D");
						length++;
						break;
					case 5:
						fprintf(output_file,"\\W");
						length++;
						break;
					case 6:
						c0=randint(1,4);
						switch(c0){
							case 1:
								fprintf(output_file,"[a-z]"); break;				//小写字符
							case 2:
								fprintf(output_file,"[\\x20\\x40\\x30]"); break;	//
							case 3:
								fprintf(output_file,"[0-9]"); break;				//数字字符
							case 4:
								fprintf(output_file,"[^\\x0A]"); break;				//非换行符
						}
						length++;
						break;
					case 7:
						fprintf(output_file,".");
						length++;
						break;
					case 8:
						c0=randint('a','z');
						fprintf(output_file,"%c+",c0);
						length++;
						break;
					case 9:
						fprintf(output_file,"\\s+");
						length++;
						break;
					case 10:
						fprintf(output_file,"\\d+");
						length++;
						break;
					case 11:
						fprintf(output_file,"\\w+");
						length++;
						break;
					case 12:
						fprintf(output_file,"\\S+");
						length++;
						break;
					case 13:
						fprintf(output_file,"\\D+");
						length++;
						break;
					case 14:
						fprintf(output_file,"\\W+");
						length++;
						break;	
					case 15:
						c0=randint(1,4);
						switch(c0){
							case 1:
								fprintf(output_file,"[a-z]+"); break;
							case 2:
								fprintf(output_file,"[\\x20\\x40\\x30]+"); break;
							case 3:
								fprintf(output_file,"[0-9]+"); break;		
							case 4:
								fprintf(output_file,"[^\\x0A]+"); break;			
						}
						length++;
						break;
					case 16:
						fprintf(output_file,".*");
						length++;
						break;
					case 17:
						fprintf(output_file,"[^\\n\\r]*");
						length++;
						break;
					case 18:
						c0=randint(0,num_long_tokens-1);
						fprintf(output_file,"(%s)+",long_tokens[c0]);
						length+=(strlen(long_tokens[c0]));
						break;
					case 19:	
						c0=randint('a','z');
						c1=randint(10,200);
						fprintf(output_file,"%c{%d}",c0,c1);
						length+=c1;
						break;	
					case 20:
						c0=randint(1,4);
						c1=randint(10,200);
						switch(c0){
							case 1:
								fprintf(output_file,"[a-z]{%d}",c1); break;
							case 2:
								fprintf(output_file,"[\\x20\\0x40\\0x30]{%d}",c1); break;
							case 3:
								fprintf(output_file,"[0-9]{%d}",c1); break;		
							case 4:
								fprintf(output_file,"[^\\x0A]{%d}",c1); break;			
						}
						length+=c1;
						break;
					case 21:
						c0=randint(0,num_long_tokens-1);
						c1=randint(10,200);
						fprintf(output_file,"(%s){%d}",long_tokens[c0],c1);
						length+=(strlen(long_tokens[c0]))*c1;
						break;
					case 22:
						c0=randint(10,200);
						fprintf(output_file,"[^\\n\\r]{%d}",c0); 
						length+=c0;
						break;
					case 23:
						c0=randint(1,features[f]);
						i=randint(0,num_short_tokens-1);
						fprintf(output_file,"(%s",short_tokens[i]);
						for (int j=0;j<c0;j++) fprintf(output_file,"|%s",short_tokens[(i+j+1)%num_short_tokens]);
						fprintf(output_file,")");
						features[f]-=(c0-1);
						break;
					default:
					 	fprintf(output_file,"\n%d !!!\n",f);
						fatal("regexgen:: impossible selection");						
				}
				features[f]--;
				num_features--; 
			}
		}
		fprintf(output_file,"\n");
		fflush(output_file);
	}
}		
