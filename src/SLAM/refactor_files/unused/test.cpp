// #include <vector>
// #include <iostream>
// #include "data_passing.hh"
// #include "multi_data_passing.hh"
// #include <condition_variable>
// #include <chrono>
// #include <thread>
// #include <mutex>

// using namespace std;

// DataPassingDeque<int> test1;
// DataPassingDeque<string> test2;
// MultiDataPassing<int, string, bool, float> test3;

// void subscriber(condition_variable* cv)
// {
//     std::mutex mtx;
//     unique_lock<mutex> lock(mtx);
//     // Give 3 chances total to check the queue because need to stop the thread eventually b/c are testing
//     for(int i=0; i<3; i++)
//     {
//         cout<<"going to sleep"<<endl;
//         cv->wait_for(lock, std::chrono::seconds(1));

//         cout<<"awake!"<<endl;
//         // loop until all queues empty
//         while(true)
//         {
//             // Lock 
//             test1.lock();
//             // If queue is empty, unlock and go back to waiting
//             if(test1.queueEmpty())
//             {
//                 test1.unlock();
//                 // break;
//                 return;
//             }
//             else // If queue has a value, process and continue the loop
//             {
//                 auto result = test1.getValue();
//                 test1.unlock();
//                 cout<<*(result.first)<<" "<<result.second.time_since_epoch().count()<<endl;
//             }
//         }
//     }
// }

// void subscriber2(condition_variable* cv)
// {
//     std::mutex mtx;
//     unique_lock<mutex> lock(mtx);
//     // Give 3 chances total to check the queue because need to stop the thread eventually b/c are testing
//     for(int i=0; i<3; i++)
//     {
//         cout<<"going to sleep"<<endl;
//         cv->wait_for(lock, std::chrono::seconds(1));

//         cout<<"awake!"<<endl;
//         // loop until all queues empty
//         while(true)
//         {
//             // Lock 
//             test2.lock();
//             // If queue is empty, unlock and go back to waiting
//             if(test2.queueEmpty())
//             {
//                 test2.unlock();
//                 break;
//                 // return;
//             }
//             else // If queue has a value, process and continue the loop
//             {
//                 auto result = test2.getValue();
//                 test2.unlock();
//                 cout<<*(result.first)<<" "<<result.second.time_since_epoch().count()<<endl;
//             }
//         }
//     }
// }

// void subscriber3(condition_variable* cv)
// {
//     std::mutex mtx;
//     unique_lock<mutex> lock(mtx);
//     // Give 3 chances total to check the queue because need to stop the thread eventually b/c are testing
//     for(int i=0; i<3; i++)
//     {
//         cout<<"going to sleep"<<endl;
//         cv->wait_for(lock, std::chrono::seconds(1));

//         cout<<"awake!"<<endl;
//         // loop until all queues empty
//         while(true)
//         {
//             // Lock 
//             test3.lock();
//             // If queue is empty, unlock and go back to waiting
//             if(test3.queueEmpty())
//             {
//                 test3.unlock();
//                 break;
//                 // return;
//             }
//             else // If queue has a value, process and continue the loop
//             {
//                 auto result = test3.getValues();
//                 test3.unlock();
//                 cout<<"--------------------printing test3 result!"<<endl;
//                 cout<<*(std::get<0>(result).first)<<" "<<std::get<0>(result).second.time_since_epoch().count()<<endl;
//                 cout<<*(std::get<1>(result).first)<<" "<<std::get<1>(result).second.time_since_epoch().count()<<endl;
//                 cout<<*(std::get<2>(result).first)<<" "<<std::get<2>(result).second.time_since_epoch().count()<<endl;
//                 cout<<*(std::get<3>(result).first)<<" "<<std::get<3>(result).second.time_since_epoch().count()<<endl;
//             }
//         }
//     }
// }


// int main()
// {
//     std::condition_variable cv;

//     // Testing basic one with ints
//     std::cout<<"---------TEST 1"<<endl;
//     test1 = DataPassingDeque<int>(3, &cv);
//     std::shared_ptr<int> five = make_shared<int>(5);
//     std::shared_ptr<int> four = make_shared<int>(4);
//     std::shared_ptr<int> six = make_shared<int>(6);
//     std::shared_ptr<int> seven = make_shared<int>(7);
    
//     thread t = std::thread(subscriber, &cv);
//     std::this_thread::sleep_for(std::chrono::milliseconds(100));

//     test1.add(five, std::chrono::steady_clock::now());
//     test1.add(four, std::chrono::steady_clock::now());
//     test1.add(six, std::chrono::steady_clock::now());
//     test1.add(seven, std::chrono::steady_clock::now());
//     test1.add(five, std::chrono::steady_clock::now());

//     t.join();
//     cout<<"TEST 1 DONE"<<endl;

//     // Testing with strings
//      std::cout<<"---------TEST 2"<<endl;
//     test2 = DataPassingDeque<string>(4, &cv);
//     std::shared_ptr<string> ta = make_shared<string>("ta");
//     std::shared_ptr<string> tuh = make_shared<string>("tuh");
//     std::shared_ptr<string> taj = make_shared<string>("taj");
//     std::shared_ptr<string> xdd = make_shared<string>("xdd");
    
//     thread t2 = std::thread(subscriber2, &cv);
//     std::this_thread::sleep_for(std::chrono::milliseconds(100));

//     test2.add(ta, std::chrono::steady_clock::now());
//     test2.add(tuh, std::chrono::steady_clock::now());
//     test2.add(taj, std::chrono::steady_clock::now());
//     test2.add(xdd, std::chrono::steady_clock::now());
//     test2.add(ta, std::chrono::steady_clock::now());

//     t2.join();
//     cout<<"TEST2 DONE"<<endl;

//     // Multipassing tests
//     std::cout<<"---------TEST 3: multipassing stuff"<<endl;
//     test3 = MultiDataPassing<int, string, bool, float>(10, 10, 4, &cv);
//     thread t3 = std::thread(subscriber3, &cv);
//     std::this_thread::sleep_for(std::chrono::milliseconds(100));

//     shared_ptr<bool> yes = make_shared<bool>(true);
//     shared_ptr<float> sixseven = make_shared<float>(6.7);
//     shared_ptr<bool> no = make_shared<bool>(false);
//     shared_ptr<float> onetwothree = make_shared<float>(1.23);

//      // add one of eacht type with current time
//     std::cout<<"about to add 1"<<endl;
//     test3.add<0>(std::pair(five, std::chrono::steady_clock::now()));
//     test3.add<1>(std::pair(ta, std::chrono::steady_clock::now()));
//     test3.add<2>(std::pair(yes, std::chrono::steady_clock::now()));
//     test3.add<3>(std::pair(sixseven, std::chrono::steady_clock::now()));
//     std::cout<<"adding 1 done"<<endl;
//     std::cout<<"adding 2"<<endl;
//     // add one of each type with same timestamp
//     std::chrono::steady_clock::time_point timing = std::chrono::steady_clock::now();
//     test3.add<1>(std::pair(tuh, timing));
//     test3.add<0>(std::pair(six, timing));
//     test3.add<3>(std::pair(onetwothree, timing));
//     test3.add<2>(std::pair(no, timing));
//     std::cout<<"adding 2 done"<<endl;
//     std::cout<<"adding 3"<<endl;
//     // add one of each type where one had bad time, then add that one again with good time. Should print xdd not taj
//     test3.add<0>(std::pair(seven, timing+timing.time_since_epoch()));
//     test3.add<3>(std::pair(sixseven, timing+timing.time_since_epoch()));
//     test3.add<1>(std::pair(taj, timing));
//     test3.add<2>(std::pair(yes, timing+timing.time_since_epoch()));
//     test3.add<1>(std::pair(xdd, timing+timing.time_since_epoch()));
//     std::cout<<"adding 3 done"<<endl;


//     std::cout<<"Testing out of order. Also adding a bunch of stuff. Lets hope it doens't crash!"<<std::endl;
//     test3.add<0>(std::pair(six, timing+timing.time_since_epoch()+timing.time_since_epoch()));
//     test3.add<1>(std::pair(tuh, timing+timing.time_since_epoch()+timing.time_since_epoch()));
//     test3.add<0>(std::pair(five, std::chrono::steady_clock::now()));
//     test3.add<0>(std::pair(seven, timing+timing.time_since_epoch()));
//     test3.add<3>(std::pair(sixseven, timing+timing.time_since_epoch()));
//     test3.add<1>(std::pair(taj, timing));
//     test3.add<2>(std::pair(yes, timing+timing.time_since_epoch()));
//     test3.add<2>(std::pair(no, timing+timing.time_since_epoch()+timing.time_since_epoch()));
//     test3.add<3>(std::pair(onetwothree, timing+timing.time_since_epoch()+timing.time_since_epoch()));
//     test3.add<1>(std::pair(ta, std::chrono::steady_clock::now()));
//     test3.add<3>(std::pair(sixseven, std::chrono::steady_clock::now()));
//     test3.add<1>(std::pair(xdd, timing+timing.time_since_epoch()));
//     test3.add<2>(std::pair(yes, std::chrono::steady_clock::now()));
//     test3.add<0>(std::pair(seven, timing+timing.time_since_epoch()));
//     test3.add<1>(std::pair(taj, timing));
//     test3.add<2>(std::pair(yes, timing+timing.time_since_epoch()));
//     test3.add<1>(std::pair(xdd, timing+timing.time_since_epoch()));
//     test3.add<3>(std::pair(sixseven, timing+timing.time_since_epoch()));
//     test3.add<3>(std::pair(onetwothree, timing));
//     test3.add<0>(std::pair(six, timing));
//     test3.add<1>(std::pair(tuh, timing));
//     test3.add<1>(std::pair(taj, timing));
//     test3.add<0>(std::pair(seven, timing+timing.time_since_epoch()));
//     test3.add<2>(std::pair(no, timing));
//     test3.add<3>(std::pair(sixseven, timing+timing.time_since_epoch()));
//     test3.add<1>(std::pair(xdd, timing+timing.time_since_epoch()));
//     test3.add<2>(std::pair(yes, timing+timing.time_since_epoch()));
//     test3.add<0>(std::pair(five, std::chrono::steady_clock::now()));
//     test3.add<1>(std::pair(ta, std::chrono::steady_clock::now()));
//    test3.add<2>(std::pair(yes, std::chrono::steady_clock::now()));
//    test3.add<3>(std::pair(sixseven, std::chrono::steady_clock::now()));
//     std::cout<<std::endl<<"out of order testing complete!"<<std::endl;

//     t3.join();
//     std::cout<<"testing 3 doine"<<endl;
// }