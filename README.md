irrlichtxml
===========

Overview
---
A c++11 wrapper for [irrlicht](http://irrlicht.sourceforge.net) xml file reader. The original reader is stream-based and you have to handle the hierarchy yourself, usually with a lot of switch. The goal of this project is to make it easier to read information such as options, from a relatively large and complicated xml file.


Usage
---
Create the reader
```c++
auto file = fs->createXMLReader("options.xml");
XMLStructure s(file);
```

Read from file
```c++
s.
	node(L"dict")->
		each(L"pair", 
			[](const INode& node) {
				std::wcout << node.str_attr(L"key") << L": " << node.str_attr(L"value") << std::endl;
			},
			[](IXMLStructure* xml) {
				xml->each(L"data", 
					[](const INode& node) {
						std::wcout << L"data\n";
					})->close();
			}
		)->
	close();
```

With a xml file like this
```xml
<dict>
	<unknown/>
	<pair key="key" value="value">
		<data/>
		<data/>
	</pair>
	<pair key="key2" value="value2"/>
</dict>
```
The program prints
```
key: value
data
data
key2: value2
```
