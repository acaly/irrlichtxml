#pragma once
#include <cassert>
#include <vector>
#include <tuple>
#include <algorithm>
#ifdef XML_USE_CPP_EXCEPTION
#include <exception>
#endif

namespace irr { namespace xml {

#ifdef XML_USE_CPP_EXCEPTION
	struct XMLEOF : std::exception {};
#endif

	typedef wchar_t XMLChar;
	struct INode
	{
		virtual const XMLChar* name() const = 0;
		virtual const XMLChar* str_attr(const XMLChar*) const = 0;
		virtual int int_attr(const XMLChar*) const = 0;
		virtual float float_attr(const XMLChar*) const = 0;
	};
	class IXMLStructure;
	typedef std::function<void(const INode&)> NodeProcessor;
	typedef std::function<void(IXMLStructure*)> StructureProcessor;

	class IXMLStructure
	{
	public:
		virtual IXMLStructure* node(const XMLChar* name) = 0;
		virtual IXMLStructure* node(const XMLChar* name, const NodeProcessor& func) = 0;
		virtual IXMLStructure* close() = 0;
		virtual IXMLStructure* each(const XMLChar* name, const NodeProcessor& func1 = NodeProcessor(), const StructureProcessor& func2 = StructureProcessor()) = 0;
		// TODO data(func)
	};

	class XMLStructure : public IXMLStructure, public INode
	{
		typedef std::tuple<const XMLChar*, NodeProcessor, StructureProcessor> EachCallback;
		typedef std::vector<EachCallback> EachCallbackList;

		// Parent object to read from.
		irr::io::IXMLReader* reader;

		// `each's created by calling `each' function.
		EachCallbackList each_callbacks;

		// Flag indicating whether last `read' operation is successful.
		// If true, will not really read from reader next time.
		bool last_read_invalid = false;

		// Current position
		int current_layer = 0;

		// To which layer is allowed to read to. Used before passing this to StructureProcessor.
		int stop_layer = -1;

	public:
		XMLStructure(irr::io::IXMLReader* reader) : reader(reader) {}

	public: // INode implementation

		virtual const XMLChar* name() const override
		{
			return reader->getNodeName();
		}
		virtual const XMLChar* str_attr(const XMLChar* name) const override
		{
			return reader->getAttributeValueSafe(name);
		}
		virtual int int_attr(const XMLChar* name) const override
		{
			return reader->getAttributeValueAsInt(name);
		}
		virtual float float_attr(const XMLChar* name) const override
		{
			return reader->getAttributeValueAsFloat(name);
		}

	public: // IXMLStructure implementation

		virtual IXMLStructure* node(const XMLChar* name) override
		{
			assert(each_callbacks.size() == 0);
			while (read())
			{
				if (reader->getNodeType() == irr::io::EXN_ELEMENT)
				{
					if (node_name_is(name))
					{
						return this;
					}
					else
					{
						skip();
					}
				}
			}
			throw_eof();
		}

		virtual IXMLStructure* node(const XMLChar* name, const NodeProcessor& func) override
		{
			assert(each_callbacks.size() == 0);
			while (read())
			{
				if (reader->getNodeType() == irr::io::EXN_ELEMENT)
				{
					if (node_name_is(name))
					{
						func(*this);
						return this;
					}
					else
					{
						skip();
					}
				}
			}
			throw_eof();
		}

		virtual IXMLStructure* close() override
		{
			do_each_to_close();
			return this;
		}

		virtual IXMLStructure* each(const XMLChar* name, const NodeProcessor& func1 = NodeProcessor(), const StructureProcessor& func2 = StructureProcessor()) override
		{
			each_callbacks.push_back(EachCallback(name, func1, func2));
			return this;
		}

	protected: // Helper functions

		int get_current_layer()
		{
			return current_layer;
		}

		// Note that when isEmptyElement, current_layer will not increase after
		// calling `read', so use this function to adjust the layer of current node
		int get_current_node_layer()
		{
			return reader->getNodeType() == irr::io::EXN_ELEMENT && reader->isEmptyElement() ?
				current_layer + 1 : current_layer;
		}

		void set_new_bound(int layer)
		{
			stop_layer = layer;
		}

		int get_current_bound()
		{
			return stop_layer;
		}

		// Step to next node.
		bool read()
		{
			if (last_read_invalid)
			{
				if (current_layer >= stop_layer)
				{
					last_read_invalid = false;
					return true;
				}
				return false;
			}
			if (reader->read())
			{
				if (reader->getNodeType() == irr::io::EXN_ELEMENT)
				{
					if (!reader->isEmptyElement()) ++current_layer;
				}
				else if (reader->getNodeType() == irr::io::EXN_ELEMENT_END)
				{
					if (--current_layer < stop_layer)
					{
						last_read_invalid = true;
						return false;
					}
				}
				return true;
			}
			return false;
		}

		bool node_name_is(const XMLChar* str)
		{
			return wcscmp(str, reader->getNodeName()) == 0;
		}

		void throw_eof()
		{
#ifdef XML_USE_CPP_EXCEPTION
			throw XMLEOF();
#endif
		}

		// Read to the end of current element
		void skip()
		{
			if (reader->isEmptyElement())
				return;
			int count = 1;
			while (read())
			{
				switch (reader->getNodeType())
				{
				case irr::io::EXN_ELEMENT:
					if (!reader->isEmptyElement())
						++count;
					break;
				case irr::io::EXN_ELEMENT_END:
					if (--count == 0) return;
					break;
				}
			}
			throw_eof();
		}

		// Handle `each's and read to the end of current element.
		void do_each_to_close()
		{
			if (last_read_invalid)
			{
				throw_eof();
			}
			// access these protected functions in lambda;
			static const auto set_new_bound = &XMLStructure::set_new_bound;
			static const auto get_current_node_layer = &XMLStructure::get_current_node_layer;
			static const auto get_current_bound = &XMLStructure::get_current_bound;

			if (reader->isEmptyElement())
				return;
			int start_layer = (this->*get_current_node_layer)();
			while (read())
			{
				switch (reader->getNodeType())
				{
				case irr::io::EXN_ELEMENT:
					if ((this->*get_current_node_layer)() == start_layer + 1)
					{
						auto name = reader->getNodeName();
						std::for_each(each_callbacks.begin(), each_callbacks.end(), 
							[=](EachCallback& e) {
								if (wcscmp(std::get<0>(e), name) == 0)
								{
									auto func1 = std::get<1>(e);
									if (func1) func1(*this);

									auto func2 = std::get<2>(e);
									if (func2 && !reader->isEmptyElement())
									{
										int b = (this->*get_current_bound)();
										(this->*set_new_bound)((this->*get_current_node_layer)());
										func2(this);
										(this->*set_new_bound)(b);
									}
								}
							});
					}
					break;
				case irr::io::EXN_ELEMENT_END:
					if ((this->*get_current_node_layer)() == start_layer - 1)
						return;
					break;
				}
			}
		}
	};
}
}
