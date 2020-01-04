#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>
#import <pthread.h>
#import "sound.h"

#define PLAY_DURATION_SECONDS 10

void* playThreadFn(void *arg) {
  char *filePath = (char*)arg;

  @autoreleasepool {

    NSString *urlString = [[NSString alloc] initWithUTF8String:filePath];
    NSURL *url = [NSURL fileURLWithPath:urlString];
    NSError *error;
    AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&error];

    if (error) {
      NSLog(@"Error in audioPlayer: %@",
            [error localizedDescription]);
    } else {
      [player play];
    }

    sleep(PLAY_DURATION_SECONDS);
  }

  return NULL;
}

void playSound(char *filePath) {
  pthread_t threadId;
  pthread_create(&threadId, NULL, playThreadFn, filePath);
}

//int main(int argc, const char * argv[]) {
//  @autoreleasepool {
//    char *path = "/Users/ddcmhenry/dev/funtastic/branches/thunder/sound/drone_ok.mp3";
//    play(path);
//    return 0;
//  }
//}