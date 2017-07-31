project "Test_SoftCompute"

   if _OPTIONS["ios"] then
   	kind "WindowedApp"
   else	
   	kind "ConsoleApp"
   end
   
   
   includedirs { "../src", "../third_party/Catch/include"}
   
   language "C++"
   
   flags { "c++11" }
   
   files {
   	"test-*.cc",
   	"test-*.h",
   }
   
   links { "SoftCompute" }

   configuration { "linux" }
      links { "dl", "pthread" }

   configuration { "macosx" }
