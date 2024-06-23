
def configure(conf, env):
    print("Configuring TerarkDB storage engine module")
    if not conf.CheckCXXHeader("terark/fsa/fsa.hpp"):
        print("Could not find <terark/fsa/fsa.hpp>, required for TerarkDB storage engine build.")
        env.Exit(1)
