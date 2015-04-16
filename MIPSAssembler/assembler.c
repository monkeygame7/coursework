#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "hash_table.h"
#include "tokenizer.h"

/* 
 * Project 1 - assembler.c
 * Shayan Motevalli
 * 11/7/2011
 * Assembler for MIPS Assembley language
 */

#define MAX_LEN 256 //maximum length for a line of code
#define INST_LEN 32 //length of an instruction

int isKeyWord(char *word);
int isRType(char *word);
int isIType(char *word);
int isJType(char *word);
int getRegNum(char *reg);
void makeLA(hash_table_t *hashTable, char *line, FILE *fptr);
void makeRType(char *inst, char *line, FILE *fptr);
void makeIType(hash_table_t *hashTable, int lineNum, char *inst, char *line, FILE *fptr);
void makeJType(hash_table_t *hashTable, char *inst, char *line, FILE *fptr);
void makeBinary(int num, int length, FILE *fptr);

int main(int argc, char *argv[])
{
	int line = 0; //the current line of the file
	int *tempLine = NULL; //holds the allocated location of the line for the hash table
	int dataOffset = 0; //the position of the data in data section
	int text_or_data = 0; //0 means currently in text block, 1 means currently in data block
	char currentLine[MAX_LEN + 1]; //holds individual line of code, to be split
	char *src = argv[1]; //file being read
	char *dest = argv[2]; //file to be written to
	char *token, *tkn_ptr = NULL; //points to current token and rest of string
	hash_table_t *hashTable = create_hash_table(255);
	FILE *inFile = fopen(src, "r");
	FILE *outFile;

	if (inFile == NULL) //invalid file inputs
	{
		printf("cannot open %s, compile failed\n", src);
		return(-1);
	}

	// first pass, finds labels and puts them in hash table

	while (fgets(currentLine, MAX_LEN, inFile)) //loop to go through file
	{
		currentLine[MAX_LEN] = 0; //ends line with null terminator
		tkn_ptr = currentLine;

		token = parse_token(tkn_ptr, " \r\n\t:", &tkn_ptr, NULL); //gets first token

		if (token == NULL || *token == '#') //line is a comment or empty
		{
			free(token);
			continue; //go to next line
		}

		if (strcmp(token, ".text") == 0) //beginning of a data section
		{
			text_or_data = 0;
			free(token);
			continue; //doesn't increment line length
		}
		else if (strcmp(token, ".data") == 0) //beginning of a text section
		{
			text_or_data = 1;
			free(token);
			continue; //doesn't increment line length
		}
		if (isKeyWord(token) == 0) //line is a label
		{
			if(hash_find(hashTable, token, strlen(token) + 1) != NULL) //already in hash table
			{
				printf("duplicate label on line %d\n", line);
				free(token);
				return(-1);
			}

			tempLine = (int *) malloc(sizeof(int)); //allocates space for the line
			if (text_or_data == 1) //inside data section
			{
				*tempLine = dataOffset + 0x2000;
				hash_insert(hashTable, token, strlen(token) + 1, tempLine);
				free(token);
				token = parse_token(tkn_ptr, " ", &tkn_ptr, NULL); //gets the type
				if (strcmp(token, ".word") == 0) //is a word
				{
					free(token);

					// gets the next part of token, if it ends with a ":", it requires one more parse
					free(parse_token(tkn_ptr, ": #\r\n\t", &tkn_ptr, NULL)); //ignores first number for now
					token = parse_token(tkn_ptr, " \t\r\n", &tkn_ptr, NULL); //if there is a second part, this will go to the following if statement
					if (token == NULL || *token == '#') //there is no second part
					{
						dataOffset += 4;
					}
					else
					{
						dataOffset += atoi(token) * 4; //offsets data section by size of array
					}
					free(token);
					continue;
				}
				else if (strcmp(token, ".asciiz") == 0) //is a string
				{
					free(token);
					token = parse_token(tkn_ptr, "\"", &tkn_ptr, NULL); //gets the string
					dataOffset += (strlen(token) + 1) / 4; //+1 for null pointer
					if ((strlen(token) + 1) % 4 != 0) //there is a remainder, round up
					{
						dataOffset++;
					}
					free(token);
					continue;
				}
				else
				{
					printf("invalid data type on line %d\n", line);
					free(token);
					return(-1);
				}
			}
			else if (text_or_data == 0) //text section
			{
				*tempLine = line;
				hash_insert(hashTable, token, strlen(token) + 1, tempLine);
				free(token);
				continue;
			}
		}
		else 
		{
			line += 4; //increments line
			if (strcmp(token, "la") == 0) //la instruction is two regular instructions, so line goes up by 2
			{
				line += 4;
			}
			free(token);
			continue;
		}
		free(token);
	}
	fclose(inFile);

	inFile = fopen(src, "r");
	outFile = fopen(dest, "w");
	line = 0; //starts at beginning again

	// second pass, writes code to file
	while (fgets(currentLine, MAX_LEN, inFile)) 
	{
		tkn_ptr = currentLine;
		token = parse_token(tkn_ptr, " \r\n\t", &tkn_ptr, NULL); //gets first token

		if (token == NULL || *token == '#' || isKeyWord(token) == 0) //line is not a key word or comment
		{
			free(token);
			continue; //go to next line
		}

		line += 4; //next line, because PC is increased before instruction is executed

		if (strcmp(token, "la") == 0)
		{
			line += 4; //la is 2 lines
			makeLA(hashTable, tkn_ptr, outFile);
			free(token);
			continue;
		}
		else if(isRType(token) == 1)
		{
			makeRType(token, tkn_ptr, outFile);
			free(token);
			continue;
		}
		else if(isIType(token) == 1)
		{
			makeIType(hashTable, line, token, tkn_ptr, outFile);
			free(token);
			continue;
		}
		else if(isJType(token) == 1)
		{
			//makeJType(hashTable, token, tkn_ptr, outFile);
			free(token);
			continue;
		}
		else
		{
			printf("invalid instruction on line %d\n", line);
			free(token);
			return(-1);
		}
		free(token);
	}
	fclose(inFile); //close the file
	fputs("\n", outFile); //blank line before data section

	inFile = fopen(src, "r");

	text_or_data = 0; //resets text/data section

	// third pass, writes data section to end

	while (fgets(currentLine, MAX_LEN, inFile)) //loop to go through file
	{
		currentLine[MAX_LEN] = 0; //ends line with null terminator
		tkn_ptr = currentLine;

		token = parse_token(tkn_ptr, " #\r\n\t:", &tkn_ptr, NULL); //gets first token

		if (token == NULL || *token == '#') //line is a comment or empty
		{
			free(token);
			continue; //go to next line
		}

		if (*token == '.') //first character is period
		{
			if (strcmp(token, ".data") == 0) //beginning of a data section
			{
				text_or_data = 1;
				free(token);
				continue;
			}
			else if (strcmp(token, ".text") == 0) //beginning of a text section
			{
				text_or_data = 0;
				free(token);
				continue;
			}
			else
			{
				printf("error on line %d\n", line);
				free(token);
				return(-1);
			}
		}
		else if (isKeyWord(token) == 0) //line is a label
		{
			if (text_or_data == 1) //inside data section
			{
				int wordValue = 0; //hold value to store for word
				int arrSize = 1; //size of array, defaulted to just 1
				free(token);
				token = parse_token(tkn_ptr, " ", &tkn_ptr, NULL); //gets the type
				if (strcmp(token, ".word") == 0) //is a word
				{
					free(token);
					//gets the next part of token, if it ends with a ":", it requires one more parse
					token = parse_token(tkn_ptr, ": #\r\n\t", &tkn_ptr, NULL);
					wordValue = atoi(token);
					free(token);
					token = parse_token(tkn_ptr, " \t\r\n", &tkn_ptr, NULL);
					if (token != NULL && *token != '#') //there is a second part
					{
						arrSize = atoi(token); //size of array
					}
					int k;
					for (k = 0; k < arrSize; k++) //repeats for size of array
					{
						makeBinary(wordValue, 32, outFile);
						fputs("\n", outFile);
					}
					free(token);
					continue;
				}
				else if (strcmp(token, ".asciiz") == 0) //is a string
				{
					free(token);
					token = parse_token(tkn_ptr, "\"", &tkn_ptr, NULL); //gets string
					tkn_ptr = token; //used to go through string
					int remaining = strlen(tkn_ptr) + 1; //includes null pointer at end
					if (remaining == 1) //empty string
					{
						fputs("00000000000000000000000000000000\n", outFile);
					}
					else
					{
						while(remaining != 0)
						{
							if (remaining >= 4) //more than 4 left
							{
								tkn_ptr += 3;
								makeBinary((int) *tkn_ptr, 8, outFile);
								tkn_ptr--;
								makeBinary((int) *tkn_ptr, 8, outFile);
								tkn_ptr--;
								makeBinary((int) *tkn_ptr, 8, outFile);
								tkn_ptr--;
								makeBinary((int) *tkn_ptr, 8, outFile);
								fputs("\n", outFile);
								tkn_ptr += 4;
								remaining -= 4;
							}
							else if(remaining == 3)
							{
								fputs("00000000", outFile); //empty space
								tkn_ptr += 2;
								makeBinary((int) *tkn_ptr, 8, outFile);
								tkn_ptr--;
								makeBinary((int) *tkn_ptr, 8, outFile);
								tkn_ptr--;
								makeBinary((int) *tkn_ptr, 8, outFile);
								fputs("\n", outFile);
								tkn_ptr += 3;
								remaining -= 3;
							}
							else if(remaining == 2)
							{
								fputs("0000000000000000", outFile); //empty space
								tkn_ptr += 1;
								makeBinary((int) *tkn_ptr, 8, outFile);
								tkn_ptr--;
								makeBinary((int) *tkn_ptr, 8, outFile);
								fputs("\n", outFile);
								tkn_ptr += 2;
								remaining -= 2;
							}
							else if(remaining == 1)
							{
								fputs("000000000000000000000000", outFile); //empty space
								makeBinary((int) *tkn_ptr, 8, outFile);
								fputs("\n", outFile);
								tkn_ptr += 1;
								remaining -= 1;
							}
						}
					}

					free(token);
					continue;
				}
				else
				{
					printf("invalid data type on line %d\n", line);
					free(token);
					return(-1);
				}
			}
		}
		free(token);
	}

	fclose(inFile);
	fclose(outFile);
	destroy_hash_table(hashTable);

	return(0);
}

int isKeyWord(char *word)
{
	/* checks all key words */
	if (strcmp(word, "la") != 0 && strcmp(word, "lw") != 0 && strcmp(word, "sw") != 0 &&
			strcmp(word, "add") != 0 && strcmp(word, "sub") != 0 && strcmp(word, "addi") != 0 &&
			strcmp(word, "or") != 0 &&  strcmp(word, "and") != 0 &&	strcmp(word, "ori") != 0 &&
			strcmp(word, "andi") != 0 && strcmp(word, "slt") != 0 && strcmp(word, "slti") != 0 &&
			strcmp(word, "sll") != 0 && strcmp(word, "srl") != 0 && strcmp(word, "beq") != 0 &&
			strcmp(word, "j") != 0 && strcmp(word, "jr") != 0 && strcmp(word, "jal") != 0)
	{
		return(0);
	}
	else
		return(1);
}

int isRType(char *word)
{
	if (strcmp(word, "add") == 0 || strcmp(word, "sub") == 0 || strcmp(word, "or") == 0 ||  
			strcmp(word, "and") == 0 || strcmp(word, "slt") == 0 || strcmp(word, "sll") == 0 ||
			strcmp(word, "srl") == 0 || strcmp(word, "jr") == 0)
	{
		return(1);
	}
	return(0);
}

int isIType(char *word)
{
	if (strcmp(word, "lw") == 0 || strcmp(word, "sw") == 0 ||
			strcmp(word, "addi") == 0 || strcmp(word, "ori") != 0 || 
			strcmp(word, "andi") == 0 || strcmp(word, "slti") != 0 ||	strcmp(word, "beq") == 0)
	{
		return(1);
	}
	return(0);
}

int isJType(char *word)
{
	if (strcmp(word, "j") == 0 || strcmp(word, "jal") == 0)
	{
		return(1);
	}
	return(0);
}

//returns the integer number of the register 
int getRegNum(char *regString)
{
	printf("%s = ", regString);
	int regnum = 0; //the actual register number
	int temp = 0; //temporary register for integer operations
	char *reg = regString + 1; //skips past dollar sign
	if (strcmp(reg, "zero") == 0)
	{
		regnum = 0;
	}
	if (strcmp(reg, "at") == 0)
	{
		regnum = 1;
	}
	else if (strcmp(reg, "gp") == 0)
	{
		regnum = 28;
	}
	else if (strcmp(reg, "sp") == 0)
	{
		regnum = 29;
	}
	else if (strcmp(reg, "ra") == 0)
	{
		regnum = 31;
	}
	else if (*reg == 'v')
	{
		regnum = 1; //starts at 1
		reg++; //moves to next one
		regnum += atoi(reg);
	}
	else if (*reg == 'a')
	{
		regnum = 4; //starts at 4
		reg++; //moves to next one
		regnum += atoi(reg);
	}
	else if (*reg == 't')
	{
		regnum = 8; //starts at 1
		reg++; //moves to next one
		temp = atoi(reg);
		if (temp > 7)
		{
			regnum += 8;
		}
		regnum += temp;
	}
	else if (*reg == 's')
	{
		regnum = 16; //starts at 1
		reg++; //moves to next one
		temp = atoi(reg);
		regnum += temp;
		if (temp == 8)
		{
			regnum = 30;
		}
	}
	else if (*reg == 'k')
	{
		regnum = 26; //starts at 1
		reg++; //moves to next one
		regnum += atoi(reg);
	}
	printf("%d\n", regnum);
	return(regnum);
}

//inserts a string with the LA instruction 
void makeLA(hash_table_t *hashTable, char *line, FILE *fptr)
{
	char *token= NULL;
	int *labelLine; //pointer to the line of the label
	int upper, lower, reg; //holds the first and last 16 bits of the label and register number

	fputs("00111100000", fptr); //beginning of opcode for lui

	token = parse_token(line, ", \t\r\n#", &line, NULL); //gets register
	reg = getRegNum(token);
	free(token);
	makeBinary(reg, 5, fptr);

	token = parse_token(line, " \r\t\n#", &line, NULL); //gets label
	labelLine = (int *) hash_find(hashTable, token, strlen(token) + 1);
	free(token);
	if (labelLine == NULL)
	{
		printf("invalid label \"%s\"\n", token);
		return;
	}
	upper = *labelLine >> 16;
	lower = *labelLine << 16;
	lower = lower >> 16;

	makeBinary(upper, 16, fptr);
	fputs("\n", fptr); //done with lui part

	fputs("001101", fptr);

	makeBinary(reg, 5, fptr);

	makeBinary(reg, 5, fptr);

	makeBinary(lower, 16, fptr);
	fputs("\n", fptr);
}

//inserts an R type instruction to the file
void makeRType(char *inst, char *line, FILE *fptr)
{
	int rs, rt, rd, sa, function = 0; //different parts of instruction
	char *token = NULL; //points to current token
	fputs("000000", fptr); //opcode for R Type

	if (strcmp(inst, "add") == 0 || strcmp(inst, "sub") == 0 || strcmp(inst, "or") == 0 || strcmp(inst, "and") == 0 || 
			strcmp(inst, "slt") == 0) //all formed the same way
	{
		token = parse_token(line, ", \n\t\r#", &line, NULL);
		rd = getRegNum(token);
		free(token);

		token = parse_token(line, ", \n\t\r#", &line, NULL);
		rs = getRegNum(token);
		free(token);

		token = parse_token(line, ", \n\t\r#", &line, NULL);
		rt = getRegNum(token);
		free(token);

		if (strcmp(inst, "add") == 0)
		{
			function = 32; //becomes 100000 in binary
		}
		else if (strcmp(inst, "sub") == 0)
		{
			function = 34; //becomes 100010 in binary
		}
		else if (strcmp(inst, "or") == 0)
		{
			function = 37; //becomes 100101 in binary
		}
		else if (strcmp(inst, "and") == 0)
		{
			function = 36; //becomes 100100 in binary
		}
		else // slt
		{
			function = 42; //becomes 101010 in binary
		}
	}
	else if (strcmp(inst, "sll") == 0 || strcmp(inst, "srl") == 0)
	{

		token = parse_token(line, ", \n\t\r#", &line, NULL);
		rd = getRegNum(token);
		free(token);

		token = parse_token(line, ", \n\t\r#", &line, NULL);
		rt = getRegNum(token);
		free(token);

		token = parse_token(line, ", \n\t\r#", &line, NULL);
		sa = atoi(token);
		free(token);

		if (strcmp(inst, "srl") == 0)
		{
			function = 2; // 000010 in binary
		}
	}
	else if (strcmp(inst, "jr") == 0)
	{
		token = parse_token(line, ", \n\t\r#", &line, NULL);
		rs = getRegNum(token);
		free(token);

		function = 8; //001000 in binary
	}

	makeBinary(rs, 5, fptr);
	makeBinary(rt, 5, fptr);
	makeBinary(rd, 5, fptr);
	makeBinary(sa, 5, fptr);
	makeBinary(function, 6, fptr);
	fputs("\n", fptr);
}

//inserts an I type instruction to the file
void makeIType(hash_table_t *hashTable, int lineNum, char *inst, char *line, FILE *fptr)
{
	int rs, rt, imm; //different parts of instruction
	int *labelLine; //holds line coming from hash table
	char *token = NULL; //points to current token

	if (strcmp(inst, "lw") == 0 || strcmp(inst, "sw") == 0) //all formed the same way
	{
		if (strcmp(inst, "lw") == 0)
		{
			fputs("100011", fptr);
		}
		else
		{
			fputs("101011", fptr);
		}

		token = parse_token(line, ", \n\t\r#", &line, NULL);
		rt = getRegNum(token);
		free(token);

		token = parse_token(line, "(", &line, NULL);
		imm = atoi(token);
		free(token);

		token = parse_token(line, ")", &line, NULL);
		rs = getRegNum(token);
		free(token);
	}
	else if (strcmp(inst, "addi") == 0 || strcmp(inst, "ori") == 0 || strcmp(inst, "andi") == 0 || 
			strcmp(inst, "slti") == 0)
	{
		if (strcmp(inst, "addi") == 0)
		{
			fputs("001000", fptr);
		}
		else if (strcmp(inst, "ori") == 0)
		{
			fputs("001101", fptr);
		}
		else if (strcmp(inst, "andi") == 0)
		{
			fputs("001100", fptr);
		}
		else if (strcmp(inst, "slti") == 0)
		{
			fputs("001100", fptr);
		}

		token = parse_token(line, ", \n\t\r#", &line, NULL);
		rt = getRegNum(token);
		free(token);

		token = parse_token(line, ", \n\t\r#", &line, NULL);
		rs = getRegNum(token);
		free(token);

		token = parse_token(line, ", \n\t\r#", &line, NULL);
		imm = atoi(token);
		free(token);
	}
	else if (strcmp(inst, "beq") == 0)
	{
		fputs("000100", fptr); //opcode

		token = parse_token(line, ", \n\t\r#", &line, NULL);
		rs = getRegNum(token);
		free(token);

		token = parse_token(line, ", \n\t\r#", &line, NULL);
		rt = getRegNum(token);
		free(token);

		token = parse_token(line, ", \n\t\r#", &line, NULL); //gets label
		labelLine = (int *) hash_find(hashTable, token, (strlen(token) + 1)); //gets corresponding line number
		free(token);

		if (labelLine == NULL)
		{
			printf("invalid label \"%s\"\n", token);
			return;
		}

		imm = *labelLine - lineNum; //offset from PC
	}

	makeBinary(rs, 5, fptr);
	makeBinary(rt, 5, fptr);
	makeBinary(imm, 16, fptr);
	fputs("\n", fptr);
}

//inserts an J type instruction to the file
void makeJType(hash_table_t *hashTable, char *inst, char *line, FILE *fptr)
{
	int target; //different parts of instruction
	int *labelLine; //holds line coming from hash table
	char *token = NULL; //points to current token

	if (strcmp(inst, "j") == 0)
	{
		fputs("000010", fptr);
	}
	else
	{
		fputs("000011", fptr);
	}

	token = parse_token(line, " \t\n\r#", &line, NULL); //gets label
	labelLine = (int *) hash_find(hashTable, token, strlen(token) + 1); //corresponding line number
	target = *labelLine;
	target = target << 4;
	target = target >> 6; //gets rid of top four bits, and bottom two

	makeBinary(target, 27, fptr);
	fputs("\n", fptr);
}

void makeBinary(int num, int length, FILE *fptr)
{
	char result[length + 1];
	result[length] = 0;
	int k;

	for (k = length - 1; k >= 0; k--) //goes through each bit
	{
		if ((1 << k) & num)
		{
			result[length - 1 - k] = '1';
		}
		else
		{
			result[length - 1 - k] = '0';
		}
	}

	fputs(result, fptr);
}
