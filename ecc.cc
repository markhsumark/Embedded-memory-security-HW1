#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <map>
uint8_t parityBits = 0;
void printBinary(char* s){
    for(int i=0; i<strlen(s); i++){
        printf("%c ", s[i]);
    }
    printf("\n");
    return;
}

int findMinR(int m) {
    int r = 0;
    while (pow(2, r) < m + r + 1) {
        r++;
    }
    return r;
}
int calBitsLen(unsigned size){
    return size * sizeof(uint8_t) * 8;
}
char* dataToBinary(uint8_t* data, unsigned size){
    // todo
    // 為二進制字符串分配內存
    int binLength = calBitsLen(size);
    char *binaryString = (char *)malloc((binLength + 1) * sizeof(char));
    if (binaryString == NULL) {
        printf("Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // 轉換為二進制字符串
    for(size_t x = 0; x<size; x++){
        uint8_t* n = data+x;
        for (int i = 0; i < 8; i++) {
            binaryString[binLength - 1 - 8*(size-x-1) - i] = ((*n >> i) & 1) ? '1' : '0';
        }
    }
    binaryString[binLength] = '\0'; // 添加字符串結束符

    return binaryString;
}

void hammingEncode(uint64_t id, uint8_t* data, unsigned size){
    int databits_len = calBitsLen(size);    // 計算整個data的bit數量
    int hamming_len = findMinR(databits_len);   // 計算需要的額外bit數量

    int bitPosition = 1;
    int dataIndex = 0;

    char* binaryOfdata = dataToBinary(data, size); // 將整個data轉成char*。(ex: 16 -> 00010000)
    
    while (dataIndex < databits_len) {
        if ((bitPosition & (bitPosition - 1)) != 0) { // 不是2的幂次方，是数据位
            if(binaryOfdata[dataIndex] == '1'){
                parityBits ^= bitPosition;
            }
            dataIndex++;
        }
        bitPosition++;
    }
    printf("hamming bits is %lld\n", parityBits);

    return;
}

void hammingDecode(uint64_t id, uint8_t* data, unsigned size){
    int databits_len = calBitsLen(size);
    int hamming_len = findMinR(databits_len);

    char* binaryOfdata = dataToBinary(data, size);
    printf("data before decode(in bits): ");
    printBinary(binaryOfdata);

    uint8_t checkBits = 0;

    int bitPosition = 1;
    int dataIndex = 0;
    
    while (dataIndex < databits_len) {
        if ((bitPosition & (bitPosition - 1)) != 0) { // 不是2的幂次方，是数据位
            if(binaryOfdata[dataIndex] == '1'){
                checkBits ^= bitPosition;
            }
            dataIndex++;
        }
        bitPosition++;
    }
    int hammingBitsIndex = 1;
    while(hammingBitsIndex <= hamming_len){
        if( (parityBits & uint8_t( 1 << (hamming_len - hammingBitsIndex))) != 0){
            checkBits^= uint8_t(pow(2, hamming_len - hammingBitsIndex));
        }
        hammingBitsIndex++;
    }
    // 修復
    if(checkBits != 0){
        double result = sqrt(checkBits) +1;
        int flip_bit_i = checkBits- (int)result -1;
        int block_i = flip_bit_i/8;
        int index = flip_bit_i%8;
        uint8_t fix_number = uint8_t(  1 << 8-index-1 );
        *(data+block_i) ^= fix_number;
    }

    return;
}



int main() {
    unsigned size = 4;
    int num[size] = {0, 11, 4, 60};
    printf("origin data: ");
    uint8_t* data = (uint8_t*)(malloc(sizeof(uint8_t)*8 *size));
    for(int i=0; i<size; i++){
        *(data+i) = num[i];
        printf("%d ", num[i]);
    }
    printf("\n");

    hammingEncode(uint64_t(1),data, size);

    // 修改一個bit
    printf("modify a bit\n");
    printf("-------\n");
    *data ^= 0x01;
    // 解碼並修正
    printf("before decode, data is ");
    for(int i=0; i<size; i++)
        printf("%d ", data[i]);
    printf("\n");
    hammingDecode(uint64_t(1),data, size);
    printf("after decode, data is ");
    for(int i=0; i<size; i++)
        printf("%d ", data[i]);
    printf("\n");
}
