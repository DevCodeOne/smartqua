import { Component, createContext } from 'preact';

export const ScaleContext = createContext({});

class ScaleContextProvider extends Component {

    state = {
        apiAddress: "",
        containedCo2: 0,
    };

    setApiAddress = (address) => {
        this.setState(state => ({
            ...state,
            apiAddress: address
        }))
    };

    setContainedCo2 = (newContainedCo2) => {
        this.setState(state => ({
            ...state,
            containedCo2: newContainedCo2
        }))
    };

    render() {
        const { setApiAddress, setContainedCo2 } = this
        const { apiAddress, containedCo2 } = this.state
        return (
            <ScaleContext.Provider value={{apiAddress, containedCo2, setApiAddress, setContainedCo2}}>
                {this.props.children}
            </ScaleContext.Provider>
        )
    }
}

export default ScaleContext

export { ScaleContextProvider }
