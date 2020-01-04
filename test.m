#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>

@interface Listener : NSObject <AVAudioPlayerDelegate>
- (void)audioPlayerDidFinishPlaying:(AVAudioPlayer *)player
                       successfully:(BOOL)flag;
- (void)audioPlayerDecodeErrorDidOccur:(AVAudioPlayer *)player
                                 error:(NSError *)error;
- (void)play:(char *)path;

@property (nonatomic, strong) AVAudioPlayer* player;
@end

@implementation Listener
@synthesize player = mPlayer;
- (void)audioPlayerDidFinishPlaying:(AVAudioPlayer *)data successfully:(BOOL)flag {
  printf("DONE\n");
//  NSLog(@"done!");
}

- (void)audioPlayerDecodeErrorDidOccur:(AVAudioPlayer *)player error:(NSError *)error
{
  NSLog(@"%s error=%@", __PRETTY_FUNCTION__, error);
}

- (void)play:(char *)path {

  NSString *urlString = [[NSString alloc] initWithUTF8String:path];
  NSURL *url = [NSURL fileURLWithPath:urlString];
  NSError *error;
  self.player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&error];

  self.player.delegate = self;

  if (error) {
    NSLog(@"Error in audioPlayer: %@",
          [error localizedDescription]);
  } else {
    [self.player play];
  }

}
@end

//void play(char* path) {
//  @autoreleasepool {
//
//    NSString *urlString = [[NSString alloc] initWithUTF8String:path];
//    NSURL *url = [NSURL fileURLWithPath:urlString];
//    NSError *error;
//    AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&error];
//
//    player.delegate = [[Listener alloc] init];
//
//    if (error) {
//      NSLog(@"Error in audioPlayer: %@",
//            [error localizedDescription]);
//    } else {
//      [player play];
//    }
//
//
//
//    sleep(10);
//  }
//}

int main(int argc, const char * argv[]) {
  @autoreleasepool {
    Listener *l = [[Listener alloc] init];
    char *path = "/Users/ddcmhenry/dev/funtastic/branches/thunder/sound/drone_ok.mp3";
    [l play:path];

    sleep(100000);
//  play("/Users/ddcmhenry/dev/funtastic/branches/thunder/sound/drone_ok.mp3");
    return 0;
  }
}