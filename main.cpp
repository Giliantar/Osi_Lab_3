#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <algorithm>

using namespace std;

mutex mtx;
condition_variable cv_main;
condition_variable cv_markers;

vector<int> arr;
int arrSize = 0;
int totalMarkers = 0;
int activeMarkers = 0;
int blockedMarkers = 0;
bool startFlag = false;
int markerToTerminate = -1;
bool continueSignal = false;

void markerFunc(int id) {
    {
        unique_lock<mutex> lock(mtx);
        while (!startFlag) {
            cv_markers.wait(lock);
        }
    }
    
    srand(id);
    
    while (true) {
        int randNum = rand();
        int index = randNum % arrSize;
        
        {
            unique_lock<mutex> lock(mtx);
            if (arr[index] == 0) {
                lock.unlock();
                this_thread::sleep_for(chrono::milliseconds(5));
                {
                    lock_guard<mutex> lock2(mtx);
                    if (arr[index] == 0) {
                        arr[index] = id;
                    } else {
                        lock2.~lock_guard();
                        continue;
                    }
                }
                this_thread::sleep_for(chrono::milliseconds(5));
                continue;
            } else {
                // Cannot place marker
                int markedCount = 0;
                for (int i = 0; i < arrSize; ++i) {
                    if (arr[i] == id) {
                        ++markedCount;
                    }
                }
                cout << "Marker " << id << ": marked elements = " << markedCount 
                     << ", cannot mark index " << index << endl;
                ++blockedMarkers;
                cv_main.notify_one();
                
                // Wait for command from main
                while (markerToTerminate != id && !continueSignal) {
                    cv_markers.wait(lock);
                }
                
                if (markerToTerminate == id) {
                    // Terminate: clear all elements marked by this thread
                    for (int i = 0; i < arrSize; ++i) {
                        if (arr[i] == id) {
                            arr[i] = 0;
                        }
                    }
                    --activeMarkers;
                    lock.unlock();
                    return;
                }
                
                if (continueSignal) {
                    --blockedMarkers;
                    lock.unlock();
                    continue;
                }
                
                lock.unlock();
            }
        }
    }
}

void printArray() {
    lock_guard<mutex> lock(mtx);
    for (int i = 0; i < arrSize; ++i) {
        cout << arr[i] << " ";
    }
    cout << endl;
}

int main() {
    cout << "Enter array size: ";
    cin >> arrSize;
    
    arr.assign(arrSize, 0);
    
    cout << "Enter number of marker threads: ";
    cin >> totalMarkers;
    
    activeMarkers = totalMarkers;
    blockedMarkers = 0;
    startFlag = false;
    markerToTerminate = -1;
    continueSignal = false;
    
    vector<thread> threads(totalMarkers);
    for (int i = 0; i < totalMarkers; ++i) {
        threads[i] = thread(markerFunc, i + 1);
    }
    
    {
        lock_guard<mutex> lock(mtx);
        startFlag = true;
    }
    cv_markers.notify_all();
    
    while (activeMarkers > 0) {
        {
            unique_lock<mutex> lock(mtx);
            cv_main.wait(lock, [&] { return blockedMarkers == activeMarkers; });
        }
        
        // Print array
        for (int i = 0; i < arrSize; ++i) {
            cout << arr[i] << " ";
        }
        cout << endl;
        
        // Request which marker to terminate
        cout << "Enter marker number to terminate: ";
        int toTerminate;
        cin >> toTerminate;
        
        {
            lock_guard<mutex> lock(mtx);
            markerToTerminate = toTerminate;
            continueSignal = false;
        }
        cv_markers.notify_all();
        
        // Wait for the marker thread to finish
        if (toTerminate >= 1 && toTerminate <= totalMarkers && threads[toTerminate - 1].joinable()) {
            threads[toTerminate - 1].join();
        }
        
        // Print array after termination
        for (int i = 0; i < arrSize; ++i) {
            cout << arr[i] << " ";
        }
        cout << endl;
        
        {
            lock_guard<mutex> lock(mtx);
            markerToTerminate = -1;
            blockedMarkers = 0;
            continueSignal = true;
        }
        cv_markers.notify_all();
    }
    
    return 0;
}