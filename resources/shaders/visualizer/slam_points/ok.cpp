// // #define SDL_MAIN_USE_CALLBACKS
// // #include <SDL3/SDL_main.h>
// // #include <SDL3/SDL.h>
// #include <fstream>
// #include <vector>
// #include <iostream>
// using namespace std;

// int main()
// {
// //   size_t vertexCodeSize; 
// //   void* vertexCode = SDL_LoadFile("shaders/vertex.spv", &vertexCodeSize);
// //     Uint8* yep = (Uint8*)vertexCode;
// //     for(int i=0; i<vertexCodeSize; i++)
// //     {
// //         cout<<*yep<<endl;
// //         yep++;
// //     }


//     std::ifstream file("slam_frag.spv", std::ios::binary);

//     if (!file) {
//         std::cerr << "Error opening file!" << std::endl;
//         return 1;
//     }

//     // 2. Determine file size
//     file.seekg(0, std::ios::end);
//     std::streamsize size = file.tellg();
//     file.seekg(0, std::ios::beg);

//     // 3. Allocate a buffer (e.g., std::vector<char>)
//     // std::vector<char> buffer(size);
//     char* arr = new char[size];


//     // 4. Read the data
//     if (file.read(arr, size)) {
//         // Successfully read 'size' bytes
//     }

//     file.close();

//     unsigned int* yep = (unsigned int *)arr;
//     int count = 0;
//     for(int i=0; i<size/4; i++)
//     {
//         if(count%8==0)
//             cout<<endl;
//         std::cout<<"0x"<<std::hex<<*yep<<", ";
//         yep++;
//         count++;
//     }
// }