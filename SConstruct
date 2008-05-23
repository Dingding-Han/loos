# Top-level SConstruct
# (c) 2008 Tod D. Romo
#
# Grossfield Lab
# Department fo Biochemistry & Biophysics
# University of Rochester Medical School
#
#



### Compile-flags

debug_opts='-g -Wall'
release_opts='-O3'

env = Environment(tools = ["default", "doxygen"], toolpath = '.')


# Determine what kind of build...
release=ARGUMENTS.get('release', 0)
if int(release):
    env.Append(CCFLAGS=release_opts)
else:
    env.Append(CCFLAGS=debug_opts)

Export('env')


# Add dirs to build in here...
SConscript('./SConscript')

