#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10); // CE, CSN

const byte address[][6] = {"1Node","2Node"};
bool isFirstNode = false; // Set false on second Arduino

char plainText[32];
char decrypted[32];
byte encrypted[32];
byte received[32];

// XTEA key (128-bit)
uint32_t xteaKey[4] = {0x01234567,0x89ABCDEF,0xFEDCBA98,0x76543210};

// --- XTEA Encryption/Decryption Functions ---
void xteaEncrypt(uint32_t v[2], uint32_t const key[4]) {
  uint32_t v0=v[0], v1=v[1], sum=0;
  uint32_t delta=0x9E3779B9;
  for(int i=0;i<32;i++){
    v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    sum += delta;
    v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
  }
  v[0]=v0; v[1]=v1;
}

void xteaDecrypt(uint32_t v[2], uint32_t const key[4]) {
  uint32_t v0=v[0], v1=v[1], sum=0xC6EF3720;
  uint32_t delta=0x9E3779B9;
  for(int i=0;i<32;i++){
    v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11)&3]);
    sum -= delta;
    v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
  }
  v[0]=v0; v[1]=v1;
}

// --- Encrypt message in 8-byte blocks ---
void encryptMessage(char* input, byte* output) {
  int len = strlen(input);
  memset(output, 0, 32); // zero padding
  for(int i=0;i<32;i+=8){
    uint32_t block[2] = {0,0};
    int copyLen = min(8, len-i);
    if(copyLen > 0) memcpy(block,input+i,copyLen);
    xteaEncrypt(block,xteaKey);
    memcpy(output+i,block,8);
  }
}

// --- Decrypt message in 8-byte blocks ---
void decryptMessage(byte* input, char* output, int msgLen){
  for(int i=0;i<32;i+=8){
    uint32_t block[2];
    memcpy(block,input+i,8);
    xteaDecrypt(block,xteaKey);
    memcpy(output+i,block,8);
  }
  output[msgLen] = '\0'; // Null-terminate
}

void printEncrypted(byte* data, int len){
  for(int i=0;i<len;i++){
    if(data[i]<16) Serial.print("0"); // For nicer formatting
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void setup() {
  Serial.begin(9600);
  radio.begin();
  radio.setPALevel(RF24_PA_HIGH);

  if(isFirstNode){
    radio.openWritingPipe(address[1]);
    radio.openReadingPipe(1,address[0]);
  } else {
    radio.openWritingPipe(address[0]);
    radio.openReadingPipe(1,address[1]);
  }

  radio.startListening();
  Serial.println("XTEA-secure chat ready. Type message (max 32 chars):");
}

void loop() {
  // --- Receive ---
  if(radio.available()){
    radio.read(&received,sizeof(received));

    Serial.print("Encrypted received (HEX): ");
    printEncrypted(received,32);

    decryptMessage(received,decrypted,32);
    Serial.print("Decrypted received: ");
    for(int i=0;i<32;i++){
      if(decrypted[i]=='\0') break;
      Serial.print(decrypted[i]);
    }
    Serial.println();
  }

  // --- Send ---
  if(Serial.available()>0){
    String input = Serial.readStringUntil('\n');
    input.toCharArray(plainText,sizeof(plainText));

    encryptMessage(plainText,encrypted);

    Serial.print("Encrypted sent (HEX): ");
    printEncrypted(encrypted,32);

    radio.stopListening();
    radio.write(&encrypted,sizeof(encrypted));
    radio.startListening();

    Serial.print("You: ");
    Serial.println(input);
  }
}
