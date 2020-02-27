#ifndef byte_utils_h
#define byte_utils_h

int char2int(char input){
  if(input >= '0' && input <= '9')
    return input - '0';
  if(input >= 'A' && input <= 'F')
    return input - 'A' + 10;
  if(input >= 'a' && input <= 'f')
    return input - 'a' + 10;
  //throw std::invalid_argument("Invalid input string");
}

// This function assumes src to be a zero terminated sanitized string with
// an even number of [0-9a-f] characters, and target to be sufficiently large
void hex2bin(const char* src, char* target){
  while(*src && src[1]){
    *(target++) = char2int(*src)*16 + char2int(src[1]);
    src += 2;
  }
}

#endif
