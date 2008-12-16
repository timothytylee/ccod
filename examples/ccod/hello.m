#!/usr/bin/ccod
<?#
/*
 Hello World (hello.m)
 The simple hello world example in Objective-C
*/
@interface HelloWorld : NSObject
{
	
}
@end

@implementation HelloWorld

- (void)hello
{
	printf("Hello World\n");
}

@end
#?>
<?

HelloWorld *hw = [[HelloWorld alloc] init];
[hw hello];

?>
