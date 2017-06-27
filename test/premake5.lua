project "Test_SoftCompute"

if _OPTIONS["ios"] then
	kind "WindowedApp"
else	
	kind "ConsoleApp"
end


includedirs { "../src", "../third_party/Catch/include"}

language "C++"

files {
	"test-*.cc",
	"test-*.h",
}

links { "SoftCompute" }
