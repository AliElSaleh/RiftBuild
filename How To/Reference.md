

# Default Options
#### Syntax
```
default.options         some options
default.options:windows bill_guts debug
default.options:linux   unleash_penguin_army debug
default.options:macOS   release enable_tim_apple_mode
```

 the default options will be completely ignored if the user has specified one or more command line options.

 running something like this: `riftbuild some_arg` will cause the default options to not be used

 but 

 running something like this: `riftbuild` by itself with no command line options will cause the default options to be used (whatever the author of the .build file has specified)




# Preset Options
#### Syntax
```
preset.debug         gcc debug asan
preset.banana:macOS  enable_banana_mode max_bananas=1000
```
 preset options can be defined if you have a lot of options that can get long and messy to type out for various configurations of you program. or perhaps the user does not know which one to use or what best suits them.

 running `riftbuild help` will present a list of preset options (if available).
 
 to use a preset, do `riftbuild preset=name` where `name` is the name of the preset that is specified in the .build file.

 for example, this:

    riftbuild debug some_option another_option max_apples=100 enable_asan alwaysfullscreen

 now becomes this:

    # in the .build file
    .preset.apples debug some_option another_option max_apples=100 enable_asan alwaysfullscreen

    # on the command line
    riftbuild preset=apples

 additionally, you can add on extra options on top of a given preset like so:

    riftbuild preset=apples some_arg another_one=8 etc.
 
 the order of these arguments do not matter

 Opinion: you should avoid structuring your software around many options like this where possible, and rarely use this feature, if you can help it. this feature is here to help shorten the command line and to make the users lives easier by not having to think about the correct combination of options to use.

 


# Preset Options Requirements (todo delete)
 1. error if specified a preset that does not exist in the build file
 2. preset options are just expanded alongside other arguments
 3. auto gen list for help display