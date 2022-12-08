/********************************************************************************
 * @file libcsvr_signal.c
 * @author Ergi (yuharsenergi@gmail.com)
 * @brief Collection functions of signal handling in csvr runtime
 * @version 0.1
 * @date 2022-12-06
 * 
 * @copyright Copyright (c) 2022 Yuharsen Ergi
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
********************************************************************************/
#include <string.h>
#include <signal.h>
#include <semaphore.h>

/**
 * @brief The csvr semaphore signal variable.
 * 
 */
static sem_t _csvrSemaphore;

/**
 * @brief This is callback function when the libcsvr is run using asyncronous method by calling csvrServerStart
 * 
 */
static void csvrSignalCallback()
{
    sem_post(&_csvrSemaphore);
}

/**
 * @brief This function will initialize the sigaction to catch the SIGINT signal and SIGTERM signal during runtime process.
 * 
 * @return This function will return the result of sem_init process.
 */
int csvrInitSignal()
{
    struct sigaction sigintHandler;
    struct sigaction sigtermHandler;

    /* Signal handler for SIGINT */
    memset(&sigintHandler, 0, sizeof(sigintHandler));
    sigintHandler.sa_sigaction = &csvrSignalCallback;
    sigintHandler.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &sigintHandler, NULL);

    /* Signal handler for SIGTERM */
    memset(&sigtermHandler, 0, sizeof(sigtermHandler));
    sigtermHandler.sa_sigaction = &csvrSignalCallback;
    sigtermHandler.sa_flags = SA_SIGINFO;
    sigaction(SIGTERM, &sigtermHandler, NULL);

    return sem_init(&_csvrSemaphore, 0, 0);
}

/**
 * @brief Function to catch the signal by waiting in the sem_wait function.
 * 
 * @return This function will return the result of sem_wait process.
 */
int csvrWaitSignal()
{
    return sem_wait(&_csvrSemaphore);
}

/**
 * @brief This function will destroy the sem_t object that has been initialized by sem_init.
 * 
 * @return This function will return the result of sem_destroy process.
 */
int csvrDestroySignal()
{
    return sem_destroy(&_csvrSemaphore);
}