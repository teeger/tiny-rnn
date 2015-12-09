/*
    Copyright (c) 2015 Peter Rudenko

    Permission is hereby granted, free of charge, to any person obtaining
    a copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the Software
    is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
    OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef TINYRNN_NETWORK_H_INCLUDED
#define TINYRNN_NETWORK_H_INCLUDED

#include "Common.h"
#include "SerializedObject.h"
#include "SerializationKeys.h"
#include "Layer.h"
#include "TrainingContext.h"
#include "Id.h"
#include "ScopedTimer.h"

#if TINYRNN_OPENCL_ACCELERATION
#include "HardcodedTrainingContext.h"
#include "HardcodedNetwork.h"
#endif

namespace TinyRNN
{
    class Network final : public SerializedObject
    {
    public:
        
        using Ptr = std::shared_ptr<Network>;
        using WeakPtr = std::weak_ptr<Network>;
        
    public:
        
        explicit Network(TrainingContext::Ptr targetContext);
        
        Network(const std::string &networkName,
                TrainingContext::Ptr targetContext,
                Layer::Ptr inputLayer,
                Layer::Vector hiddenLayers,
                Layer::Ptr outputLayer);
        
        std::string getName() const noexcept;
        Id getUuid() const noexcept;
        
        TrainingContext::Ptr getContext() const noexcept;
        
        // Feed the input layer, process the rest and get result values from the output
        Neuron::Values feed(const Neuron::Values &input);
        
        // Back-propagation magic
        void train(Value rate, const Neuron::Values &target);
        
        // Connections
        Neuron::Connection::HashMap connectAllToAll(Network::Ptr other);
        Neuron::Connection::HashMap connectOneToOne(Network::Ptr other);
        
        // Gating
        bool gateAllIncomingConnections(Network::Ptr toNetwork, const Neuron::Connection::HashMap &connections);
        bool gateAllOutgoingConnections(Network::Ptr fromNetwork, const Neuron::Connection::HashMap &connections);
        bool gateOneToOne(Network::Ptr fromNetwork, Network::Ptr toNetwork, const Neuron::Connection::HashMap &connections);
        
    public:
        
        struct Prefabs
        {
            static Network::Ptr feedForward(const std::string &name,
                                            int inputLayerSize,
                                            const std::vector<int> &hiddenLayersSizes,
                                            int outputLayerSize);
            
            static Network::Ptr longShortTermMemory(const std::string &name,
                                                    int inputLayerSize,
                                                    const std::vector<int> &hiddenLayersSizes,
                                                    int outputLayerSize);
        };
        
    public:
        
        virtual void deserialize(SerializationContext::Ptr context) override;
        virtual void serialize(SerializationContext::Ptr context) const override;
        
#if TINYRNN_OPENCL_ACCELERATION
        HardcodedNetwork::Ptr hardcode() const;
        void restore(HardcodedTrainingContext::Ptr context);
#endif
        
    private:
        
        std::string name;
        Id uuid;
        
        Layer::Ptr inputLayer;
        Layer::Vector hiddenLayers;
        Layer::Ptr outputLayer;
        
        TrainingContext::Ptr context;
        
    private:
        
        Neuron::Connection::SortedMap findAllConnections() const;
        Neuron::Ptr findNeuronWithId(const Id &uuid);
        
    private:
        
        TINYRNN_DISALLOW_COPY_AND_ASSIGN(Network);
    };
    
    //===------------------------------------------------------------------===//
    // Network implementation
    //===------------------------------------------------------------------===//
    
    inline Network::Network(TrainingContext::Ptr targetContext) :
    uuid(Uuid::generateId()),
    context(targetContext)
    {
    }
    
    inline Network::Network(const std::string &networkName,
                     TrainingContext::Ptr targetContext,
                     Layer::Ptr targetInputLayer,
                     Layer::Vector targetHiddenLayers,
                     Layer::Ptr targetOutputLayer) :
    name(networkName),
    uuid(Uuid::generateId()),
    inputLayer(targetInputLayer),
    hiddenLayers(targetHiddenLayers),
    outputLayer(targetOutputLayer),
    context(targetContext)
    {
    }
    
    inline std::string Network::getName() const noexcept
    {
        return this->name;
    }
    
    inline Id Network::getUuid() const noexcept
    {
        return this->uuid;
    }
    
    inline TrainingContext::Ptr Network::getContext() const noexcept
    {
        return this->context;
    }
    
    //===------------------------------------------------------------------===//
    // Core
    //===------------------------------------------------------------------===//
    
    inline Neuron::Values Network::feed(const Neuron::Values &input)
    {
        this->inputLayer->feed(input);
        
        for (auto &hiddenLayer : this->hiddenLayers)
        {
            hiddenLayer->process();
        }
        
        const Neuron::Values &result = this->outputLayer->process();
        return result;
    }
    
    inline void Network::train(Value rate, const Neuron::Values &target)
    {
        this->outputLayer->train(rate, target);
        
        for (size_t i = this->hiddenLayers.size(); i --> 0 ;)
        {
            this->hiddenLayers[i]->backPropagate(rate);
        }
    }
    
    //===------------------------------------------------------------------===//
    // Connections
    //===------------------------------------------------------------------===//
    
    inline Neuron::Connection::HashMap Network::connectAllToAll(Network::Ptr other)
    {
        return this->outputLayer->connectAllToAll(other->inputLayer);
    }
    
    inline Neuron::Connection::HashMap Network::connectOneToOne(Network::Ptr other)
    {
        return this->outputLayer->connectOneToOne(other->inputLayer);
    }
    
    inline bool Network::gateAllIncomingConnections(Network::Ptr toNetwork, const Neuron::Connection::HashMap &connections)
    {
        return this->outputLayer->gateAllIncomingConnections(toNetwork->inputLayer, connections);
    }
    
    inline bool Network::gateAllOutgoingConnections(Network::Ptr fromNetwork, const Neuron::Connection::HashMap &connections)
    {
        return this->outputLayer->gateAllOutgoingConnections(fromNetwork->outputLayer, connections);
    }
    
    inline bool Network::gateOneToOne(Network::Ptr fromNetwork, Network::Ptr toNetwork, const Neuron::Connection::HashMap &connections)
    {
        return this->outputLayer->gateOneToOne(fromNetwork->outputLayer, toNetwork->inputLayer, connections);
    }
    
    //===------------------------------------------------------------------===//
    // Serialization
    //===------------------------------------------------------------------===//
    
    inline void Network::deserialize(SerializationContext::Ptr context)
    {
        this->uuid = context->getNumberProperty(Keys::Core::Uuid);
        this->name = context->getStringProperty(Keys::Core::Name);
        
        this->inputLayer.reset();
        SerializationContext::Ptr inputLayerNode(context->getChildContext(Keys::Core::InputLayer));
        this->inputLayer = Layer::Ptr(new Layer(this->context, 0));
        this->inputLayer->deserialize(inputLayerNode);
        
        this->outputLayer.reset();
        SerializationContext::Ptr outputLayerNode(context->getChildContext(Keys::Core::OutputLayer));
        this->outputLayer = Layer::Ptr(new Layer(this->context, 0));
        this->outputLayer->deserialize(outputLayerNode);
        
        this->hiddenLayers.clear();
        SerializationContext::Ptr allHiddenLayersNode(context->getChildContext(Keys::Core::HiddenLayers));
        
        for (size_t i = 0; i < allHiddenLayersNode->getNumChildrenContexts(); ++i)
        {
            SerializationContext::Ptr hiddenLayerNode(allHiddenLayersNode->getChildContext(i));
            Layer::Ptr layer(new Layer(this->context, 0));
            layer->deserialize(hiddenLayerNode);
            this->hiddenLayers.push_back(layer);
        }
        
        SerializationContext::Ptr connectionsNode(context->getChildContext(Keys::Core::Connections));
        
        for (size_t i = 0; i < connectionsNode->getNumChildrenContexts(); ++i)
        {
            SerializationContext::Ptr connectionNode(connectionsNode->getChildContext(i));
            
            Neuron::Connection::Ptr connection(new Neuron::Connection(this->context));
            connection->deserialize(connectionNode);
            
            const Id inputNeuronUuid = connectionNode->getNumberProperty(Keys::Core::InputNeuronUuid);
            const Id gateNeuronUuid = connectionNode->getNumberProperty(Keys::Core::GateNeuronUuid);
            const Id outputNeuronUuid = connectionNode->getNumberProperty(Keys::Core::OutputNeuronUuid);
            
            Neuron::Ptr inputNeuron(this->findNeuronWithId(inputNeuronUuid));
            Neuron::Ptr outputNeuron(this->findNeuronWithId(outputNeuronUuid));
            
            connection->connect(inputNeuron, outputNeuron);
            
            if (gateNeuronUuid > 0)
            {
                Neuron::Ptr gateNeuron(this->findNeuronWithId(gateNeuronUuid));
                gateNeuron->gate(connection);
            }
        }
    }
    
    inline void Network::serialize(SerializationContext::Ptr context) const
    {
        context->setNumberProperty(this->uuid, Keys::Core::Uuid);
        context->setStringProperty(this->name, Keys::Core::Name);
        
        SerializationContext::Ptr inputLayerNode(context->createChildContext(Keys::Core::InputLayer));
        this->inputLayer->serialize(inputLayerNode);
        
        SerializationContext::Ptr allHiddenLayersNode(context->createChildContext(Keys::Core::HiddenLayers));
        
        for (const auto &layer : this->hiddenLayers)
        {
            SerializationContext::Ptr hiddenLayerNode(allHiddenLayersNode->createChildContext(Keys::Core::Layer));
            layer->serialize(hiddenLayerNode);
        }
        
        SerializationContext::Ptr outputLayerNode(context->createChildContext(Keys::Core::OutputLayer));
        this->outputLayer->serialize(outputLayerNode);
        
        SerializationContext::Ptr allConnectionsNode(context->createChildContext(Keys::Core::Connections));
        Neuron::Connection::SortedMap allConnections(this->findAllConnections());
        
        for (const auto &i : allConnections)
        {
            SerializationContext::Ptr connectionNode(allConnectionsNode->createChildContext(Keys::Core::Connection));
            const Neuron::Connection::Ptr connection = i.second;
            connection->serialize(connectionNode);
        }
    }
    
    inline Neuron::Connection::SortedMap Network::findAllConnections() const
    {
        Neuron::Connection::SortedMap result;
        
        for (const auto &hiddenLayer : this->hiddenLayers)
        {
            const Neuron::Connection::HashMap layerOutgoingConnections = hiddenLayer->findAllOutgoingConnections();
            result.insert(layerOutgoingConnections.begin(), layerOutgoingConnections.end());
        }
        
        const Neuron::Connection::HashMap inputLayerOutgoingConnections = this->inputLayer->findAllOutgoingConnections();
        result.insert(inputLayerOutgoingConnections.begin(), inputLayerOutgoingConnections.end());
        
        const Neuron::Connection::HashMap outputLayersOutgoingConnections = this->outputLayer->findAllOutgoingConnections();
        result.insert(outputLayersOutgoingConnections.begin(), outputLayersOutgoingConnections.end());
        
        return result;
    }
    
    inline Neuron::Ptr Network::findNeuronWithId(const Id &uuid)
    {
        if (Neuron::Ptr neuron = this->inputLayer->getNeuronWithId(uuid))
        {
            return neuron;
        }
        
        if (Neuron::Ptr neuron = this->outputLayer->getNeuronWithId(uuid))
        {
            return neuron;
        }
        
        for (const auto &hiddenLayer : this->hiddenLayers)
        {
            if (Neuron::Ptr neuron = hiddenLayer->getNeuronWithId(uuid))
            {
                return neuron;
            }
        }
        
        return nullptr;
    }
    
#if TINYRNN_OPENCL_ACCELERATION
    
    //===------------------------------------------------------------------===//
    // Hardcoding into OpenCL kernels
    //===------------------------------------------------------------------===//
    
#if not defined TINYRNN_MAX_NUMBER_OF_EXPRESSIONS_PER_KERNEL
#define TINYRNN_MAX_NUMBER_OF_EXPRESSIONS_PER_KERNEL 10000
#endif
    
    inline HardcodedNetwork::Ptr Network::hardcode() const
    {
        HardcodedTrainingContext::Ptr context(new HardcodedTrainingContext());
        HardcodedNetwork::HardcodedLayers hardcodedLayers;
        
        {
            const ScopedTimer timer("Network::hardcode");
            hardcodedLayers.push_back(this->inputLayer->hardcode(context, true, false));
            
            for (auto &hiddenLayer : this->hiddenLayers)
            {
                hardcodedLayers.push_back(hiddenLayer->hardcode(context, false, false));
            }
            
            hardcodedLayers.push_back(this->outputLayer->hardcode(context, false, true));
        }
            
        HardcodedNetwork::Ptr hardcodedNetwork(new HardcodedNetwork(context, hardcodedLayers,
                                                                    TINYRNN_MAX_NUMBER_OF_EXPRESSIONS_PER_KERNEL));
        
        std::cout << "Hardcoded context memory size: " << context->getMemory().size() << std::endl;
        return hardcodedNetwork;
    }
    
    inline void Network::restore(HardcodedTrainingContext::Ptr context)
    {
        const ScopedTimer timer("Network::restore");
        
        this->inputLayer->restore(context);
        
        for (auto &hiddenLayer : this->hiddenLayers)
        {
            hiddenLayer->restore(context);
        }
        
        this->outputLayer->restore(context);
    }
    
#endif
    
    //===------------------------------------------------------------------===//
    // Network prefabs
    //===------------------------------------------------------------------===//
    
    inline Network::Ptr Network::Prefabs::feedForward(const std::string &name,
                                                      int inputLayerSize,
                                                      const std::vector<int> &hiddenLayersSizes,
                                                      int outputLayerSize)
    {
        TrainingContext::Ptr context(new TrainingContext(name));
        
        Layer::Ptr inputLayer = Layer::Ptr(new Layer(context, inputLayerSize));
        
        std::vector<Layer::Ptr> hiddenLayers;
        Layer::Ptr prevHiddenLayer = nullptr;
        
        for (size_t i = 0; i < hiddenLayersSizes.size(); ++i)
        {
            Layer::Ptr hiddenLayer = Layer::Ptr(new Layer(context, hiddenLayersSizes[i]));
            
            if (i == 0)
            {
                inputLayer->connectAllToAll(hiddenLayer);
            }
            else if (prevHiddenLayer != nullptr)
            {
                prevHiddenLayer->connectAllToAll(hiddenLayer);
            }
            
            prevHiddenLayer = hiddenLayer;
            hiddenLayers.push_back(hiddenLayer);
        }
        
        Layer::Ptr outputLayer(new Layer(context, outputLayerSize));
        prevHiddenLayer->connectAllToAll(outputLayer);
        
        Network::Ptr network = Network::Ptr(new Network(name, context, inputLayer, hiddenLayers, outputLayer));
        return network;
    }
    
    inline Network::Ptr Network::Prefabs::longShortTermMemory(const std::string &name,
                                                              int inputLayerSize,
                                                              const std::vector<int> &hiddenLayersSizes,
                                                              int outputLayerSize)
    {
        TrainingContext::Ptr context(new TrainingContext(name));
        
        Layer::Ptr inputLayer(new Layer(context, inputLayerSize));
        Layer::Ptr outputLayer(new Layer(context, outputLayerSize));
        
        const int numHiddenLayers = hiddenLayersSizes.size();
        Layer::Vector hiddenLayers;
        Layer::Ptr previous;
        
        for (int i = 0; i < numHiddenLayers; ++i)
        {
            const int size = hiddenLayersSizes[i];
            
            Layer::Ptr inputGate(new Layer(context, size, 1.0));
            Layer::Ptr forgetGate(new Layer(context, size, 1.0));
            Layer::Ptr memoryCell(new Layer(context, size));
            Layer::Ptr outputGate(new Layer(context, size, 1.0));
            
            hiddenLayers.push_back(inputGate);
            hiddenLayers.push_back(forgetGate);
            hiddenLayers.push_back(memoryCell);
            hiddenLayers.push_back(outputGate);
            
            const auto &input = inputLayer->connectAllToAll(memoryCell);
            inputLayer->connectAllToAll(inputGate);
            inputLayer->connectAllToAll(forgetGate);
            inputLayer->connectAllToAll(outputGate);
            
            Neuron::Connection::HashMap cell;
            
            if (previous != nullptr)
            {
                cell = previous->connectAllToAll(memoryCell);
                previous->connectAllToAll(inputGate);
                previous->connectAllToAll(forgetGate);
                previous->connectAllToAll(outputGate);
            }
            
            const auto &output = memoryCell->connectAllToAll(outputLayer);
            
            const auto &self = memoryCell->connectOneToOne(memoryCell);
            //const auto &self = memoryCell->connectAllToAll(memoryCell);
            
            // optional
            //outputLayer->connectAllToAll(memoryCell);
            
            // optional
            //outputLayer->connectAllToAll(inputGate);
            //outputLayer->connectAllToAll(outputGate);
            //outputLayer->connectAllToAll(forgetGate);
            
            // optional
            //memoryCell->connectOneToOne(inputGate);
            memoryCell->connectAllToAll(inputGate);
            memoryCell->connectAllToAll(forgetGate);
            memoryCell->connectAllToAll(outputGate);
            
            // gates
            inputGate->gateAllIncomingConnections(memoryCell, input);
            forgetGate->gateOneToOne(memoryCell, memoryCell, self);
            outputGate->gateAllOutgoingConnections(memoryCell, output);
            
            if (previous != nullptr)
            {
                inputGate->gateAllIncomingConnections(memoryCell, cell);
            }
            
            previous = memoryCell;
        }
        
        // optional
        inputLayer->connectAllToAll(outputLayer);
        
        Network::Ptr network = Network::Ptr(new Network(name, context, inputLayer, hiddenLayers, outputLayer));
        return network;
    }
}

#endif // TINYRNN_NETWORK_H_INCLUDED
